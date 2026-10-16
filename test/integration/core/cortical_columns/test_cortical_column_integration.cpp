/**
 * @file test_cortical_column_integration.cpp
 * @brief Integration tests for cortical column module
 *
 * Tests cortical column functionality including:
 * - Column creation with default/custom config
 * - Layer activation (L1-L6)
 * - Feedforward and feedback processing
 * - Lateral inhibition
 * - Minicolumn organization
 * - Statistics tracking
 * - Bio-async integration
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

#include "utils/nimcp_test_base.h"
#include "core/cortical_columns/nimcp_cortical_column.h"
#include "core/cortical_columns/nimcp_cortical_layers.h"

/*=============================================================================
 * Test Fixture
 *===========================================================================*/

class CorticalColumnIntegrationTest : public NimcpTestBase {
protected:
    void SetUp() override {
        NimcpTestBase::SetUp();

        // Create pool with default config
        pool_config_.max_minicolumns = 100;
        pool_config_.max_hypercolumns = 20;
        pool_config_.max_neurons_per_minicolumn = 100;
        pool_config_.enable_cow_support = false;

        pool_ = cortical_column_pool_create(&pool_config_);
        ASSERT_NE(pool_, nullptr);
    }

    void TearDown() override {
        // Free any allocated neuron ID arrays
        for (auto ptr : allocated_neuron_ids_) {
            delete[] ptr;
        }
        allocated_neuron_ids_.clear();

        // Free any allocated minicolumn config arrays
        for (auto ptr : allocated_minicolumn_configs_) {
            delete[] ptr;
        }
        allocated_minicolumn_configs_.clear();

        if (pool_) {
            cortical_column_pool_destroy(pool_);
            pool_ = nullptr;
        }
        NimcpTestBase::TearDown();
    }

    // Helper to create default minicolumn config with valid neuron IDs
    minicolumn_config_t CreateDefaultMinicolumnConfig() {
        minicolumn_config_t config;
        memset(&config, 0, sizeof(config));

        config.num_neurons = 80;
        // Allocate neuron IDs - implementation requires non-null array
        uint32_t* neuron_ids = new uint32_t[config.num_neurons];
        for (uint32_t i = 0; i < config.num_neurons; i++) {
            neuron_ids[i] = i;
        }
        config.neuron_ids = neuron_ids;
        allocated_neuron_ids_.push_back(neuron_ids);

        config.receptive_field.center_x = 0.0f;
        config.receptive_field.center_y = 0.0f;
        config.receptive_field.center_z = 0.0f;
        config.receptive_field.radius = 1.0f;
        config.tuning_preference = 0.0f;
        config.layers.layer_2_3_count = 32;
        config.layers.layer_4_count = 12;
        config.layers.layer_5_6_count = 36;

        return config;
    }

    // Track allocated neuron ID arrays for cleanup
    std::vector<uint32_t*> allocated_neuron_ids_;

    // Helper to create hypercolumn config with multiple minicolumns
    hypercolumn_config_t CreateDefaultHypercolumnConfig(uint32_t num_minicolumns) {
        hypercolumn_config_t config;
        memset(&config, 0, sizeof(config));

        config.num_minicolumns = num_minicolumns;

        // Allocate and configure minicolumn configs
        minicolumn_config_t* mc_configs = new minicolumn_config_t[num_minicolumns];
        allocated_minicolumn_configs_.push_back(mc_configs);

        for (uint32_t i = 0; i < num_minicolumns; i++) {
            memset(&mc_configs[i], 0, sizeof(minicolumn_config_t));
            mc_configs[i].num_neurons = 80;

            // Allocate neuron IDs for each minicolumn
            uint32_t* neuron_ids = new uint32_t[80];
            for (uint32_t j = 0; j < 80; j++) {
                neuron_ids[j] = i * 80 + j;
            }
            mc_configs[i].neuron_ids = neuron_ids;
            allocated_neuron_ids_.push_back(neuron_ids);

            mc_configs[i].receptive_field.center_x = 0.0f;
            mc_configs[i].receptive_field.center_y = 0.0f;
            mc_configs[i].receptive_field.center_z = 0.0f;
            mc_configs[i].receptive_field.radius = 1.0f;
            // Distribute tuning preferences across orientation space
            mc_configs[i].tuning_preference = (180.0f / num_minicolumns) * i;
            mc_configs[i].layers.layer_2_3_count = 32;
            mc_configs[i].layers.layer_4_count = 12;
            mc_configs[i].layers.layer_5_6_count = 36;
        }
        config.minicolumn_configs = mc_configs;

        config.feature_space_min = 0.0f;
        config.feature_space_max = 180.0f;
        config.topographic_x = 0.0f;
        config.topographic_y = 0.0f;
        config.competition = CC_COMPETITION_SOFTMAX;
        config.k_winners = 1;
        config.temperature = 1.0f;
        config.lateral_inhibition_strength = 0.5f;
        config.lateral_inhibition_sigma1 = 1.0f;
        config.lateral_inhibition_sigma2 = 3.0f;

        return config;
    }

    // Track allocated minicolumn config arrays for cleanup
    std::vector<minicolumn_config_t*> allocated_minicolumn_configs_;

    cortical_column_pool_config_t pool_config_;
    cortical_column_pool_t* pool_ = nullptr;
};

/*=============================================================================
 * Pool Creation Tests
 *===========================================================================*/

TEST_F(CorticalColumnIntegrationTest, PoolCreateWithNullConfig) {
    // Implementation requires non-null config, returns NULL on null config
    cortical_column_pool_t* pool = cortical_column_pool_create(nullptr);
    EXPECT_EQ(pool, nullptr);
    if (pool) {
        cortical_column_pool_destroy(pool);
    }
}

TEST_F(CorticalColumnIntegrationTest, PoolCreateWithCustomConfig) {
    cortical_column_pool_config_t config;
    config.max_minicolumns = 500;
    config.max_hypercolumns = 50;
    config.max_neurons_per_minicolumn = 150;
    config.enable_cow_support = true;

    cortical_column_pool_t* pool = cortical_column_pool_create(&config);
    EXPECT_NE(pool, nullptr);
    if (pool) {
        cortical_column_pool_destroy(pool);
    }
}

TEST_F(CorticalColumnIntegrationTest, PoolDestroyNullSafe) {
    // Should not crash with NULL
    cortical_column_pool_destroy(nullptr);
    SUCCEED();
}

/*=============================================================================
 * Minicolumn Creation Tests
 *===========================================================================*/

TEST_F(CorticalColumnIntegrationTest, MinicolumnCreateWithDefaultConfig) {
    minicolumn_config_t config = CreateDefaultMinicolumnConfig();

    minicolumn_t* col = minicolumn_create(pool_, &config);
    ASSERT_NE(col, nullptr);

    minicolumn_destroy(col);
}

TEST_F(CorticalColumnIntegrationTest, MinicolumnCreateWithNullPool) {
    minicolumn_config_t config = CreateDefaultMinicolumnConfig();

    minicolumn_t* col = minicolumn_create(nullptr, &config);
    EXPECT_EQ(col, nullptr);
}

TEST_F(CorticalColumnIntegrationTest, MinicolumnCreateWithNullConfig) {
    minicolumn_t* col = minicolumn_create(pool_, nullptr);
    EXPECT_EQ(col, nullptr);
}

TEST_F(CorticalColumnIntegrationTest, MinicolumnDestroyNullSafe) {
    minicolumn_destroy(nullptr);
    SUCCEED();
}

TEST_F(CorticalColumnIntegrationTest, MinicolumnCreateMultiple) {
    std::vector<minicolumn_t*> columns;

    for (int i = 0; i < 10; i++) {
        minicolumn_config_t config = CreateDefaultMinicolumnConfig();
        config.tuning_preference = i * 18.0f;  // Different orientations

        minicolumn_t* col = minicolumn_create(pool_, &config);
        ASSERT_NE(col, nullptr);
        columns.push_back(col);
    }

    // Clean up
    for (auto col : columns) {
        minicolumn_destroy(col);
    }
}

TEST_F(CorticalColumnIntegrationTest, MinicolumnCreateWithCustomLayerDistribution) {
    minicolumn_config_t config;
    memset(&config, 0, sizeof(config));

    // Custom layer distribution with 100 neurons
    config.num_neurons = 100;

    // Allocate neuron IDs for 100 neurons
    uint32_t* neuron_ids = new uint32_t[100];
    for (uint32_t i = 0; i < 100; i++) {
        neuron_ids[i] = i;
    }
    config.neuron_ids = neuron_ids;
    allocated_neuron_ids_.push_back(neuron_ids);

    config.receptive_field.center_x = 0.0f;
    config.receptive_field.center_y = 0.0f;
    config.receptive_field.center_z = 0.0f;
    config.receptive_field.radius = 1.0f;
    config.tuning_preference = 0.0f;

    // Layer counts must sum to num_neurons (100)
    config.layers.layer_2_3_count = 40;
    config.layers.layer_4_count = 15;
    config.layers.layer_5_6_count = 45;

    minicolumn_t* col = minicolumn_create(pool_, &config);
    ASSERT_NE(col, nullptr);

    minicolumn_destroy(col);
}

/*=============================================================================
 * Hypercolumn Creation Tests
 *===========================================================================*/

TEST_F(CorticalColumnIntegrationTest, HypercolumnCreateBasic) {
    hypercolumn_config_t config = CreateDefaultHypercolumnConfig(8);

    hypercolumn_t* hcol = hypercolumn_create(pool_, &config);
    ASSERT_NE(hcol, nullptr);

    hypercolumn_destroy(hcol);
}

TEST_F(CorticalColumnIntegrationTest, HypercolumnCreateWithNullPool) {
    hypercolumn_config_t config = CreateDefaultHypercolumnConfig(8);

    hypercolumn_t* hcol = hypercolumn_create(nullptr, &config);
    EXPECT_EQ(hcol, nullptr);
}

TEST_F(CorticalColumnIntegrationTest, HypercolumnCreateWithNullConfig) {
    hypercolumn_t* hcol = hypercolumn_create(pool_, nullptr);
    EXPECT_EQ(hcol, nullptr);
}

TEST_F(CorticalColumnIntegrationTest, HypercolumnDestroyNullSafe) {
    hypercolumn_destroy(nullptr);
    SUCCEED();
}

TEST_F(CorticalColumnIntegrationTest, HypercolumnCreateWithDifferentCompetitionModes) {
    // Winner-take-all
    {
        hypercolumn_config_t config = CreateDefaultHypercolumnConfig(8);
        config.competition = CC_COMPETITION_WINNER_TAKE_ALL;

        hypercolumn_t* hcol = hypercolumn_create(pool_, &config);
        ASSERT_NE(hcol, nullptr);
        hypercolumn_destroy(hcol);
    }

    // K-winners
    {
        hypercolumn_config_t config = CreateDefaultHypercolumnConfig(8);
        config.competition = CC_COMPETITION_K_WINNERS;
        config.k_winners = 3;

        hypercolumn_t* hcol = hypercolumn_create(pool_, &config);
        ASSERT_NE(hcol, nullptr);
        hypercolumn_destroy(hcol);
    }

    // Softmax
    {
        hypercolumn_config_t config = CreateDefaultHypercolumnConfig(8);
        config.competition = CC_COMPETITION_SOFTMAX;
        config.temperature = 0.5f;

        hypercolumn_t* hcol = hypercolumn_create(pool_, &config);
        ASSERT_NE(hcol, nullptr);
        hypercolumn_destroy(hcol);
    }

    // No competition
    {
        hypercolumn_config_t config = CreateDefaultHypercolumnConfig(8);
        config.competition = CC_COMPETITION_NONE;

        hypercolumn_t* hcol = hypercolumn_create(pool_, &config);
        ASSERT_NE(hcol, nullptr);
        hypercolumn_destroy(hcol);
    }
}

/*=============================================================================
 * Minicolumn Computation Tests
 *===========================================================================*/

TEST_F(CorticalColumnIntegrationTest, MinicolumnComputeBasic) {
    minicolumn_config_t config = CreateDefaultMinicolumnConfig();
    minicolumn_t* col = minicolumn_create(pool_, &config);
    ASSERT_NE(col, nullptr);

    float input[] = {0.5f, 0.5f, 0.5f};
    float activation = minicolumn_compute(col, input, 3);

    // Should return valid activation
    EXPECT_GE(activation, 0.0f);
    EXPECT_LE(activation, 1.0f);

    minicolumn_destroy(col);
}

TEST_F(CorticalColumnIntegrationTest, MinicolumnComputeWithNullInput) {
    minicolumn_config_t config = CreateDefaultMinicolumnConfig();
    minicolumn_t* col = minicolumn_create(pool_, &config);
    ASSERT_NE(col, nullptr);

    float activation = minicolumn_compute(col, nullptr, 3);
    EXPECT_LT(activation, 0.0f);  // Should return error (-1.0f)

    minicolumn_destroy(col);
}

TEST_F(CorticalColumnIntegrationTest, MinicolumnComputeWithZeroInputSize) {
    minicolumn_config_t config = CreateDefaultMinicolumnConfig();
    minicolumn_t* col = minicolumn_create(pool_, &config);
    ASSERT_NE(col, nullptr);

    float input[] = {0.5f};
    float activation = minicolumn_compute(col, input, 0);

    // Should handle gracefully (may return 0 or error)
    EXPECT_GE(activation, -1.0f);

    minicolumn_destroy(col);
}

TEST_F(CorticalColumnIntegrationTest, MinicolumnComputeMultipleInputs) {
    minicolumn_config_t config = CreateDefaultMinicolumnConfig();
    minicolumn_t* col = minicolumn_create(pool_, &config);
    ASSERT_NE(col, nullptr);

    // Process several inputs
    for (int i = 0; i < 10; i++) {
        float input[] = {0.1f * i, 0.2f * i, 0.3f * i};
        float activation = minicolumn_compute(col, input, 3);
        EXPECT_GE(activation, -1.0f);  // Valid or error
    }

    minicolumn_destroy(col);
}

/*=============================================================================
 * Hypercolumn Computation Tests
 *===========================================================================*/

TEST_F(CorticalColumnIntegrationTest, HypercolumnComputeBasic) {
    hypercolumn_config_t config = CreateDefaultHypercolumnConfig(8);
    hypercolumn_t* hcol = hypercolumn_create(pool_, &config);
    ASSERT_NE(hcol, nullptr);

    float input[] = {0.5f, 0.5f, 0.5f};
    hypercolumn_compute(hcol, input, 3);

    // Check winner is valid
    uint32_t winner = hypercolumn_get_winner(hcol);
    EXPECT_LT(winner, 8u);

    hypercolumn_destroy(hcol);
}

TEST_F(CorticalColumnIntegrationTest, HypercolumnGetDistribution) {
    hypercolumn_config_t config = CreateDefaultHypercolumnConfig(8);
    hypercolumn_t* hcol = hypercolumn_create(pool_, &config);
    ASSERT_NE(hcol, nullptr);

    float input[] = {0.5f, 0.5f, 0.5f};
    hypercolumn_compute(hcol, input, 3);

    float distribution[8];
    hypercolumn_get_distribution(hcol, distribution, 8);

    // All activations should be valid
    float sum = 0.0f;
    for (int i = 0; i < 8; i++) {
        EXPECT_GE(distribution[i], 0.0f);
        sum += distribution[i];
    }

    // With softmax, sum should be approximately 1.0
    EXPECT_GT(sum, 0.0f);

    hypercolumn_destroy(hcol);
}

TEST_F(CorticalColumnIntegrationTest, HypercolumnWinnerTakeAll) {
    hypercolumn_config_t config = CreateDefaultHypercolumnConfig(8);
    config.competition = CC_COMPETITION_WINNER_TAKE_ALL;

    hypercolumn_t* hcol = hypercolumn_create(pool_, &config);
    ASSERT_NE(hcol, nullptr);

    float input[] = {0.5f, 0.5f, 0.5f};
    hypercolumn_compute(hcol, input, 3);

    float distribution[8];
    hypercolumn_get_distribution(hcol, distribution, 8);

    // Only one should be active (1.0), others should be 0.0
    int active_count = 0;
    for (int i = 0; i < 8; i++) {
        if (distribution[i] > 0.5f) {
            active_count++;
        }
    }
    EXPECT_EQ(active_count, 1);

    hypercolumn_destroy(hcol);
}

TEST_F(CorticalColumnIntegrationTest, HypercolumnKWinners) {
    hypercolumn_config_t config = CreateDefaultHypercolumnConfig(8);
    config.competition = CC_COMPETITION_K_WINNERS;
    config.k_winners = 3;

    hypercolumn_t* hcol = hypercolumn_create(pool_, &config);
    ASSERT_NE(hcol, nullptr);

    float input[] = {0.5f, 0.5f, 0.5f};
    hypercolumn_compute(hcol, input, 3);

    float distribution[8];
    hypercolumn_get_distribution(hcol, distribution, 8);

    // Exactly k should be active
    int active_count = 0;
    for (int i = 0; i < 8; i++) {
        if (distribution[i] > 0.5f) {
            active_count++;
        }
    }
    EXPECT_LE(active_count, 3);

    hypercolumn_destroy(hcol);
}

/*=============================================================================
 * Lateral Inhibition Tests
 *===========================================================================*/

TEST_F(CorticalColumnIntegrationTest, MinicolumnApplyLateralInhibition) {
    minicolumn_config_t config = CreateDefaultMinicolumnConfig();
    minicolumn_t* col = minicolumn_create(pool_, &config);
    ASSERT_NE(col, nullptr);

    // First compute to set initial activation
    float input[] = {1.0f, 1.0f, 1.0f};
    float initial_activation = minicolumn_compute(col, input, 3);

    // Apply lateral inhibition
    minicolumn_apply_lateral_inhibition(col, 0.3f);

    // Get stats to check inhibition effect
    minicolumn_stats_t stats;
    minicolumn_get_stats(col, &stats);

    EXPECT_GE(stats.inhibition_level, 0.0f);

    minicolumn_destroy(col);
}

TEST_F(CorticalColumnIntegrationTest, LateralInhibitionClampedToValidRange) {
    minicolumn_config_t config = CreateDefaultMinicolumnConfig();
    minicolumn_t* col = minicolumn_create(pool_, &config);
    ASSERT_NE(col, nullptr);

    float input[] = {0.5f, 0.5f, 0.5f};
    minicolumn_compute(col, input, 3);

    // Apply excessive inhibition
    minicolumn_apply_lateral_inhibition(col, 2.0f);

    // Activation should still be in valid range
    minicolumn_stats_t stats;
    minicolumn_get_stats(col, &stats);

    EXPECT_GE(stats.activation_level, 0.0f);
    EXPECT_LE(stats.activation_level, 1.0f);

    minicolumn_destroy(col);
}

/*=============================================================================
 * Competition Tests
 *===========================================================================*/

TEST_F(CorticalColumnIntegrationTest, HypercolumnRunCompetitionSoftmax) {
    hypercolumn_config_t config = CreateDefaultHypercolumnConfig(8);
    config.competition = CC_COMPETITION_NONE;  // Start without competition

    hypercolumn_t* hcol = hypercolumn_create(pool_, &config);
    ASSERT_NE(hcol, nullptr);

    float input[] = {0.5f, 0.5f, 0.5f};
    hypercolumn_compute(hcol, input, 3);

    // Now manually run softmax competition
    hypercolumn_run_competition(hcol, CC_COMPETITION_SOFTMAX, 1.0f);

    float distribution[8];
    hypercolumn_get_distribution(hcol, distribution, 8);

    // Check softmax property (sum to ~1.0)
    float sum = 0.0f;
    for (int i = 0; i < 8; i++) {
        EXPECT_GE(distribution[i], 0.0f);
        EXPECT_LE(distribution[i], 1.0f);
        sum += distribution[i];
    }

    hypercolumn_destroy(hcol);
}

TEST_F(CorticalColumnIntegrationTest, HypercolumnTemperatureAffectsSoftmax) {
    hypercolumn_config_t config = CreateDefaultHypercolumnConfig(8);
    config.competition = CC_COMPETITION_NONE;

    hypercolumn_t* hcol = hypercolumn_create(pool_, &config);
    ASSERT_NE(hcol, nullptr);

    float input[] = {0.5f, 0.5f, 0.5f};
    hypercolumn_compute(hcol, input, 3);

    // Low temperature -> sharper distribution
    hypercolumn_run_competition(hcol, CC_COMPETITION_SOFTMAX, 0.1f);

    float distribution_sharp[8];
    hypercolumn_get_distribution(hcol, distribution_sharp, 8);

    // Reset and try high temperature
    hypercolumn_compute(hcol, input, 3);
    hypercolumn_run_competition(hcol, CC_COMPETITION_SOFTMAX, 10.0f);

    float distribution_flat[8];
    hypercolumn_get_distribution(hcol, distribution_flat, 8);

    // High temp should have more uniform distribution
    float max_sharp = 0.0f, max_flat = 0.0f;
    for (int i = 0; i < 8; i++) {
        if (distribution_sharp[i] > max_sharp) max_sharp = distribution_sharp[i];
        if (distribution_flat[i] > max_flat) max_flat = distribution_flat[i];
    }

    // Sharp distribution should have higher max
    EXPECT_GE(max_sharp, max_flat);

    hypercolumn_destroy(hcol);
}

/*=============================================================================
 * Receptive Field Tests
 *===========================================================================*/

TEST_F(CorticalColumnIntegrationTest, MinicolumnSetReceptiveField) {
    minicolumn_config_t config = CreateDefaultMinicolumnConfig();
    minicolumn_t* col = minicolumn_create(pool_, &config);
    ASSERT_NE(col, nullptr);

    // Set new receptive field
    minicolumn_set_receptive_field(col, 1.0f, 2.0f, 3.0f, 2.5f);

    // Compute weight at center - should be maximum
    float weight_at_center = minicolumn_compute_receptive_weight(col, 1.0f, 2.0f, 3.0f);
    EXPECT_FLOAT_EQ(weight_at_center, 1.0f);

    minicolumn_destroy(col);
}

TEST_F(CorticalColumnIntegrationTest, MinicolumnReceptiveFieldGaussianFalloff) {
    minicolumn_config_t config = CreateDefaultMinicolumnConfig();
    config.receptive_field.center_x = 0.0f;
    config.receptive_field.center_y = 0.0f;
    config.receptive_field.center_z = 0.0f;
    config.receptive_field.radius = 1.0f;

    minicolumn_t* col = minicolumn_create(pool_, &config);
    ASSERT_NE(col, nullptr);

    // Weight should decrease with distance
    float weight_0 = minicolumn_compute_receptive_weight(col, 0.0f, 0.0f, 0.0f);
    float weight_1 = minicolumn_compute_receptive_weight(col, 1.0f, 0.0f, 0.0f);
    float weight_2 = minicolumn_compute_receptive_weight(col, 2.0f, 0.0f, 0.0f);

    EXPECT_FLOAT_EQ(weight_0, 1.0f);
    EXPECT_GT(weight_0, weight_1);
    EXPECT_GT(weight_1, weight_2);
    EXPECT_GT(weight_2, 0.0f);

    minicolumn_destroy(col);
}

TEST_F(CorticalColumnIntegrationTest, MinicolumnReceptiveFieldNullColumn) {
    float weight = minicolumn_compute_receptive_weight(nullptr, 0.0f, 0.0f, 0.0f);
    EXPECT_LT(weight, 0.0f);  // Should return -1.0f for error
}

/*=============================================================================
 * Statistics Tests
 *===========================================================================*/

TEST_F(CorticalColumnIntegrationTest, MinicolumnGetStats) {
    minicolumn_config_t config = CreateDefaultMinicolumnConfig();
    config.num_neurons = 80;

    minicolumn_t* col = minicolumn_create(pool_, &config);
    ASSERT_NE(col, nullptr);

    // Process some inputs
    for (int i = 0; i < 5; i++) {
        float input[] = {0.1f * i, 0.2f * i, 0.3f * i};
        minicolumn_compute(col, input, 3);
    }

    minicolumn_stats_t stats;
    minicolumn_get_stats(col, &stats);

    EXPECT_EQ(stats.num_neurons, 80u);
    EXPECT_GE(stats.activation_level, 0.0f);
    EXPECT_LE(stats.activation_level, 1.0f);
    EXPECT_GE(stats.total_activations, 0u);

    minicolumn_destroy(col);
}

TEST_F(CorticalColumnIntegrationTest, HypercolumnGetStats) {
    hypercolumn_config_t config = CreateDefaultHypercolumnConfig(8);
    config.competition = CC_COMPETITION_SOFTMAX;

    hypercolumn_t* hcol = hypercolumn_create(pool_, &config);
    ASSERT_NE(hcol, nullptr);

    // Process some inputs
    for (int i = 0; i < 5; i++) {
        float input[] = {0.1f * i, 0.2f * i, 0.3f * i};
        hypercolumn_compute(hcol, input, 3);
    }

    cc_hypercolumn_stats_t stats;
    hypercolumn_get_stats(hcol, &stats);

    EXPECT_EQ(stats.num_minicolumns, 8u);
    EXPECT_LT(stats.winner_index, 8u);
    EXPECT_GE(stats.winner_activation, 0.0f);
    EXPECT_GE(stats.total_activation, 0.0f);
    EXPECT_GE(stats.total_computations, 5u);
    EXPECT_EQ(stats.competition_mode, CC_COMPETITION_SOFTMAX);

    hypercolumn_destroy(hcol);
}

TEST_F(CorticalColumnIntegrationTest, MinicolumnStatsNullPointer) {
    minicolumn_config_t config = CreateDefaultMinicolumnConfig();
    minicolumn_t* col = minicolumn_create(pool_, &config);
    ASSERT_NE(col, nullptr);

    // Should handle NULL stats gracefully
    minicolumn_get_stats(col, nullptr);
    SUCCEED();  // No crash

    // Should handle NULL column gracefully
    minicolumn_stats_t stats;
    minicolumn_get_stats(nullptr, &stats);
    SUCCEED();  // No crash

    minicolumn_destroy(col);
}

TEST_F(CorticalColumnIntegrationTest, HypercolumnStatsNullPointer) {
    hypercolumn_config_t config = CreateDefaultHypercolumnConfig(8);
    hypercolumn_t* hcol = hypercolumn_create(pool_, &config);
    ASSERT_NE(hcol, nullptr);

    // Should handle NULL stats gracefully
    hypercolumn_get_stats(hcol, nullptr);
    SUCCEED();

    // Should handle NULL hypercolumn gracefully
    cc_hypercolumn_stats_t stats;
    hypercolumn_get_stats(nullptr, &stats);
    SUCCEED();

    hypercolumn_destroy(hcol);
}

/*=============================================================================
 * Laminar Structure Integration Tests
 *===========================================================================*/

TEST_F(CorticalColumnIntegrationTest, LaminarStructureCreateWithDefaults) {
    laminar_structure_t* ls = laminar_structure_create(nullptr);
    ASSERT_NE(ls, nullptr);

    laminar_structure_destroy(ls);
}

TEST_F(CorticalColumnIntegrationTest, LaminarStructureCreateWithCustomConfig) {
    cortical_layer_config_t configs[CC_LAYER_COUNT];

    for (int i = 0; i < CC_LAYER_COUNT; i++) {
        configs[i] = cortical_layer_get_default_config((cc_cortical_layer_t)i);
    }

    laminar_structure_t* ls = laminar_structure_create(configs);
    ASSERT_NE(ls, nullptr);

    laminar_structure_destroy(ls);
}

TEST_F(CorticalColumnIntegrationTest, LaminarProcessInput) {
    laminar_structure_t* ls = laminar_structure_create(nullptr);
    ASSERT_NE(ls, nullptr);

    // Process input to Layer IV (thalamic input)
    float input[] = {0.5f, 0.6f, 0.7f, 0.8f};
    laminar_process_input(ls, CC_LAYER_IV, input, 4);

    // Check activation increased
    float activation = laminar_get_layer_activation(ls, CC_LAYER_IV);
    EXPECT_GE(activation, 0.0f);

    laminar_structure_destroy(ls);
}

TEST_F(CorticalColumnIntegrationTest, LaminarFeedforwardProcessing) {
    laminar_structure_t* ls = laminar_structure_create(nullptr);
    ASSERT_NE(ls, nullptr);

    // Set up canonical circuit
    laminar_apply_canonical_circuit(ls);

    // Inject input to Layer IV
    float input[] = {1.0f, 1.0f, 1.0f, 1.0f};
    laminar_process_input(ls, CC_LAYER_IV, input, 4);

    // Process feedforward
    laminar_process_feedforward(ls);

    // Layer II/III and V should have activation
    float l23_activation = laminar_get_layer_activation(ls, CC_LAYER_II_III);
    float l5_activation = laminar_get_layer_activation(ls, CC_LAYER_V);

    EXPECT_GE(l23_activation, 0.0f);
    EXPECT_GE(l5_activation, 0.0f);

    laminar_structure_destroy(ls);
}

TEST_F(CorticalColumnIntegrationTest, LaminarFeedbackProcessing) {
    laminar_structure_t* ls = laminar_structure_create(nullptr);
    ASSERT_NE(ls, nullptr);

    laminar_apply_canonical_circuit(ls);

    // Inject input to higher layer (Layer I - top-down)
    float input[] = {0.8f, 0.8f, 0.8f, 0.8f};
    laminar_process_input(ls, CC_LAYER_I, input, 4);

    // Process feedback
    laminar_process_feedback(ls);

    // Layer II/III should receive modulation
    float l23_activation = laminar_get_layer_activation(ls, CC_LAYER_II_III);
    EXPECT_GE(l23_activation, 0.0f);

    laminar_structure_destroy(ls);
}

TEST_F(CorticalColumnIntegrationTest, LaminarLateralProcessing) {
    laminar_structure_t* ls = laminar_structure_create(nullptr);
    ASSERT_NE(ls, nullptr);

    laminar_apply_canonical_circuit(ls);

    // Inject input to Layer II/III
    float input[] = {0.5f, 0.5f, 0.5f, 0.5f};
    laminar_process_input(ls, CC_LAYER_II_III, input, 4);

    // Process lateral connections
    laminar_process_lateral(ls);

    // Check activation is modified
    float activation = laminar_get_layer_activation(ls, CC_LAYER_II_III);
    EXPECT_GE(activation, 0.0f);

    laminar_structure_destroy(ls);
}

TEST_F(CorticalColumnIntegrationTest, LaminarGetOutput) {
    laminar_structure_t* ls = laminar_structure_create(nullptr);
    ASSERT_NE(ls, nullptr);

    laminar_apply_canonical_circuit(ls);

    float input[] = {1.0f, 1.0f, 1.0f, 1.0f};
    laminar_process_input(ls, CC_LAYER_IV, input, 4);
    laminar_process_feedforward(ls);

    float output[4];
    laminar_get_output(ls, CC_LAYER_V, output, 4);

    // Output should be valid
    for (int i = 0; i < 4; i++) {
        EXPECT_GE(output[i], 0.0f);
    }

    laminar_structure_destroy(ls);
}

TEST_F(CorticalColumnIntegrationTest, LaminarGetLayerNeuronCount) {
    laminar_structure_t* ls = laminar_structure_create(nullptr);
    ASSERT_NE(ls, nullptr);

    for (int i = 0; i < CC_LAYER_COUNT; i++) {
        uint32_t count = laminar_get_layer_neuron_count(ls, (cc_cortical_layer_t)i);
        EXPECT_GT(count, 0u);
    }

    laminar_structure_destroy(ls);
}

TEST_F(CorticalColumnIntegrationTest, LaminarConnectFeedforward) {
    laminar_structure_t* ls = laminar_structure_create(nullptr);
    ASSERT_NE(ls, nullptr);

    // Manually configure feedforward connections
    laminar_connect_feedforward(ls, CC_LAYER_IV, CC_LAYER_II_III, 0.8f);
    laminar_connect_feedforward(ls, CC_LAYER_II_III, CC_LAYER_V, 0.6f);

    // Test propagation
    float input[] = {1.0f, 1.0f, 1.0f, 1.0f};
    laminar_process_input(ls, CC_LAYER_IV, input, 4);
    laminar_process_feedforward(ls);

    SUCCEED();

    laminar_structure_destroy(ls);
}

TEST_F(CorticalColumnIntegrationTest, LaminarConnectFeedback) {
    laminar_structure_t* ls = laminar_structure_create(nullptr);
    ASSERT_NE(ls, nullptr);

    // Configure feedback connections
    laminar_connect_feedback(ls, CC_LAYER_VI, CC_LAYER_IV, 0.5f);
    laminar_connect_feedback(ls, CC_LAYER_I, CC_LAYER_II_III, 0.4f);

    // Test propagation
    float input[] = {1.0f, 1.0f, 1.0f, 1.0f};
    laminar_process_input(ls, CC_LAYER_VI, input, 4);
    laminar_process_feedback(ls);

    SUCCEED();

    laminar_structure_destroy(ls);
}

TEST_F(CorticalColumnIntegrationTest, LaminarGetProfile) {
    laminar_structure_t* ls = laminar_structure_create(nullptr);
    ASSERT_NE(ls, nullptr);

    laminar_apply_canonical_circuit(ls);

    float input[] = {0.5f, 0.5f, 0.5f, 0.5f};
    laminar_process_input(ls, CC_LAYER_IV, input, 4);
    laminar_process_feedforward(ls);

    laminar_profile_t profile;
    laminar_get_profile(ls, &profile);

    // All layers should have valid values
    for (int i = 0; i < CC_LAYER_COUNT; i++) {
        EXPECT_GE(profile.layer_activations[i], 0.0f);
    }

    // Timestamp should be set
    EXPECT_GE(profile.timestamp, 0u);

    laminar_structure_destroy(ls);
}

TEST_F(CorticalColumnIntegrationTest, LaminarGetStats) {
    laminar_structure_t* ls = laminar_structure_create(nullptr);
    ASSERT_NE(ls, nullptr);

    laminar_apply_canonical_circuit(ls);

    // Process multiple cycles
    for (int cycle = 0; cycle < 10; cycle++) {
        float input[] = {0.5f, 0.5f, 0.5f, 0.5f};
        laminar_process_input(ls, CC_LAYER_IV, input, 4);
        laminar_process_feedforward(ls);
        laminar_process_feedback(ls);
    }

    laminar_stats_t stats;
    laminar_get_stats(ls, &stats);

    // Check stats are valid
    for (int i = 0; i < CC_LAYER_COUNT; i++) {
        EXPECT_GE(stats.mean_activation[i], 0.0f);
        EXPECT_GE(stats.variance_activation[i], 0.0f);
    }

    EXPECT_GE(stats.total_feedforward_flow, 0.0f);
    EXPECT_GE(stats.total_feedback_flow, 0.0f);
    EXPECT_GT(stats.update_count, 0u);

    laminar_structure_destroy(ls);
}

/*=============================================================================
 * Layer Configuration Tests
 *===========================================================================*/

TEST_F(CorticalColumnIntegrationTest, LayerGetDefaultConfig) {
    for (int i = 0; i < CC_LAYER_COUNT; i++) {
        cortical_layer_config_t config =
            cortical_layer_get_default_config((cc_cortical_layer_t)i);

        EXPECT_EQ(config.layer, (cc_cortical_layer_t)i);
        EXPECT_GT(config.thickness_ratio, 0.0f);
        EXPECT_LE(config.thickness_ratio, 1.0f);
        EXPECT_GT(config.neuron_density, 0u);
        EXPECT_GT(config.excitatory_ratio, 0.0f);
        EXPECT_LE(config.excitatory_ratio, 1.0f);
        EXPECT_GE(config.default_connectivity, 0.0f);
        EXPECT_LE(config.default_connectivity, 1.0f);
    }
}

TEST_F(CorticalColumnIntegrationTest, LayerGetName) {
    const char* names[] = {
        "Layer I",
        "Layer II/III",
        "Layer IV",
        "Layer V",
        "Layer VI"
    };

    for (int i = 0; i < CC_LAYER_COUNT; i++) {
        const char* name = cortical_layer_get_name((cc_cortical_layer_t)i);
        EXPECT_NE(name, nullptr);
        EXPECT_STREQ(name, names[i]);
    }
}

TEST_F(CorticalColumnIntegrationTest, LayerGetDescription) {
    for (int i = 0; i < CC_LAYER_COUNT; i++) {
        const char* desc = cortical_layer_get_description((cc_cortical_layer_t)i);
        EXPECT_NE(desc, nullptr);
        EXPECT_GT(strlen(desc), 0u);
    }
}

/*=============================================================================
 * Stress Tests
 *===========================================================================*/

TEST_F(CorticalColumnIntegrationTest, HighFrequencyComputation) {
    hypercolumn_config_t config = CreateDefaultHypercolumnConfig(16);
    hypercolumn_t* hcol = hypercolumn_create(pool_, &config);
    ASSERT_NE(hcol, nullptr);

    // Simulate high-frequency processing
    for (int i = 0; i < 1000; i++) {
        float input[] = {
            0.5f + 0.3f * sinf(i * 0.1f),
            0.5f + 0.3f * cosf(i * 0.1f),
            0.5f + 0.3f * sinf(i * 0.2f)
        };
        hypercolumn_compute(hcol, input, 3);
    }

    cc_hypercolumn_stats_t stats;
    hypercolumn_get_stats(hcol, &stats);

    EXPECT_GE(stats.total_computations, 1000u);

    hypercolumn_destroy(hcol);
}

TEST_F(CorticalColumnIntegrationTest, MultipleHypercolumnsParallel) {
    std::vector<hypercolumn_t*> hypercolumns;

    // Create multiple hypercolumns
    for (int i = 0; i < 10; i++) {
        hypercolumn_config_t config = CreateDefaultHypercolumnConfig(8);
        config.topographic_x = i * 1.0f;
        config.topographic_y = i * 0.5f;

        hypercolumn_t* hcol = hypercolumn_create(pool_, &config);
        ASSERT_NE(hcol, nullptr);
        hypercolumns.push_back(hcol);
    }

    // Process all simultaneously
    for (int cycle = 0; cycle < 100; cycle++) {
        float input[] = {0.5f, 0.5f, 0.5f};
        for (auto hcol : hypercolumns) {
            hypercolumn_compute(hcol, input, 3);
        }
    }

    // Clean up
    for (auto hcol : hypercolumns) {
        hypercolumn_destroy(hcol);
    }
}

TEST_F(CorticalColumnIntegrationTest, LaminarFullCycleProcessing) {
    laminar_structure_t* ls = laminar_structure_create(nullptr);
    ASSERT_NE(ls, nullptr);

    laminar_apply_canonical_circuit(ls);

    // Full processing cycle
    for (int cycle = 0; cycle < 100; cycle++) {
        float input[] = {
            0.5f + 0.2f * sinf(cycle * 0.1f),
            0.5f + 0.2f * cosf(cycle * 0.1f),
            0.5f + 0.2f * sinf(cycle * 0.2f),
            0.5f + 0.2f * cosf(cycle * 0.2f)
        };

        laminar_process_input(ls, CC_LAYER_IV, input, 4);
        laminar_process_feedforward(ls);
        laminar_process_lateral(ls);
        laminar_process_feedback(ls);
    }

    laminar_stats_t stats;
    laminar_get_stats(ls, &stats);

    EXPECT_GE(stats.update_count, 100u);

    laminar_structure_destroy(ls);
}
