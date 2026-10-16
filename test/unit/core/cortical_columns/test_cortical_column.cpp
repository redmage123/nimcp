/**
 * @file test_cortical_column.cpp
 * @brief Comprehensive unit tests for NIMCP cortical column module
 * @version 1.0.0
 * @date 2025-11-25
 *
 * WHAT: Full code coverage tests for cortical column architecture
 * WHY:  Ensure all functionality works correctly and edge cases are handled
 * HOW:  GTest fixtures with WHAT/WHY/HOW documentation, realistic test data
 *
 * TEST COVERAGE:
 * - Pool Management: Create/destroy with various configs
 * - Minicolumn Lifecycle: Create/destroy, validation, pool exhaustion
 * - Hypercolumn Lifecycle: All competition modes, various configurations
 * - Processing: Compute activations with math verification
 * - Competition: WTA, K-winners, Softmax, None modes
 * - Receptive Fields: Gaussian weight calculation verification
 * - Lateral Inhibition: Mexican hat function, clamping
 * - Statistics: Stats collection and accuracy
 * - Edge Cases: NULL pointers, boundary values
 * - Math Verification: Gaussian, Mexican hat, softmax formulas
 *
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <algorithm>
#include <numeric>

#include "core/cortical_columns/nimcp_cortical_column.h"

//=============================================================================
// Test Constants
//=============================================================================

constexpr float EPSILON = 1e-6f;
constexpr float TOLERANCE = 1e-5f;

//=============================================================================
// Test Fixtures
//=============================================================================

/**
 * WHAT: Base fixture for cortical column tests
 * WHY:  Provide common setup/teardown and helper functions
 * HOW:  Create pool in SetUp, destroy in TearDown
 */
class CorticalColumnTest : public ::testing::Test {
protected:
    cortical_column_pool_t* pool;

    void SetUp() override {
        pool = nullptr;
    }

    void TearDown() override {
        if (pool) {
            cortical_column_pool_destroy(pool);
            pool = nullptr;
        }
    }

    /**
     * WHAT: Create pool with default config
     * WHY:  Convenience method for common case
     */
    void CreateDefaultPool() {
        cortical_column_pool_config_t config = {
            .max_minicolumns = 100,
            .max_hypercolumns = 10,
            .max_neurons_per_minicolumn = 100,
            .enable_cow_support = false
        };
        pool = cortical_column_pool_create(&config);
        ASSERT_NE(pool, nullptr);
    }

    /**
     * WHAT: Create minicolumn configuration for testing
     * WHY:  Provide valid config for minicolumn tests
     */
    minicolumn_config_t CreateTestMinicolumnConfig(
        std::vector<uint32_t>& neuron_ids,
        float tuning_preference
    ) {
        minicolumn_config_t config = {};
        config.neuron_ids = neuron_ids.data();
        config.num_neurons = neuron_ids.size();
        config.receptive_field.center_x = 0.0f;
        config.receptive_field.center_y = 0.0f;
        config.receptive_field.center_z = 0.0f;
        config.receptive_field.radius = 1.0f;
        config.tuning_preference = tuning_preference;
        config.layers.layer_2_3_count = neuron_ids.size() / 3;
        config.layers.layer_4_count = neuron_ids.size() / 3;
        config.layers.layer_5_6_count = neuron_ids.size() -
            (config.layers.layer_2_3_count + config.layers.layer_4_count);
        return config;
    }

    /**
     * WHAT: Verify Gaussian weight calculation
     * WHY:  Ensure mathematical correctness
     * HOW:  Compare against formula: exp(-d²/2σ²)
     */
    float ComputeExpectedGaussianWeight(float distance, float sigma) {
        return std::exp(-(distance * distance) / (2.0f * sigma * sigma));
    }

    /**
     * WHAT: Verify softmax calculation
     * WHY:  Ensure mathematical correctness
     * HOW:  Compute: p_i = exp(a_i/T) / Σexp(a_j/T)
     */
    std::vector<float> ComputeExpectedSoftmax(
        const std::vector<float>& activations,
        float temperature
    ) {
        std::vector<float> result(activations.size());

        // Find max for numerical stability
        float max_val = *std::max_element(activations.begin(), activations.end());

        // Compute exp values and sum
        float sum = 0.0f;
        for (size_t i = 0; i < activations.size(); i++) {
            result[i] = std::exp((activations[i] - max_val) / temperature);
            sum += result[i];
        }

        // Normalize
        if (sum > EPSILON) {
            for (size_t i = 0; i < result.size(); i++) {
                result[i] /= sum;
            }
        }

        return result;
    }
};

//=============================================================================
// Pool Management Tests
//=============================================================================

/**
 * WHAT: Test pool creation with default config
 * WHY:  Verify basic pool creation works
 * HOW:  Create pool, verify non-NULL
 */
TEST_F(CorticalColumnTest, PoolCreate_DefaultConfig_Success) {
    cortical_column_pool_config_t config = {
        .max_minicolumns = 100,
        .max_hypercolumns = 10,
        .max_neurons_per_minicolumn = 100,
        .enable_cow_support = false
    };

    pool = cortical_column_pool_create(&config);
    ASSERT_NE(pool, nullptr);
}

/**
 * WHAT: Test pool creation with custom config
 * WHY:  Verify pool accepts various configurations
 * HOW:  Create pools with different sizes
 */
TEST_F(CorticalColumnTest, PoolCreate_CustomConfigs_Success) {
    // Small pool
    cortical_column_pool_config_t small_config = {
        .max_minicolumns = 10,
        .max_hypercolumns = 2,
        .max_neurons_per_minicolumn = 50,
        .enable_cow_support = false
    };
    pool = cortical_column_pool_create(&small_config);
    ASSERT_NE(pool, nullptr);
    cortical_column_pool_destroy(pool);

    // Large pool
    cortical_column_pool_config_t large_config = {
        .max_minicolumns = 1000,
        .max_hypercolumns = 100,
        .max_neurons_per_minicolumn = 150,
        .enable_cow_support = true
    };
    pool = cortical_column_pool_create(&large_config);
    ASSERT_NE(pool, nullptr);
}

/**
 * WHAT: Test pool creation with NULL config
 * WHY:  Verify error handling for invalid input
 * HOW:  Pass NULL, expect NULL return
 */
TEST_F(CorticalColumnTest, PoolCreate_NullConfig_ReturnsNull) {
    pool = cortical_column_pool_create(nullptr);
    EXPECT_EQ(pool, nullptr);
}

/**
 * WHAT: Test pool creation with zero sizes
 * WHY:  Verify validation of pool configuration
 * HOW:  Create config with zero minicolumns/hypercolumns
 */
TEST_F(CorticalColumnTest, PoolCreate_ZeroSizes_ReturnsNull) {
    cortical_column_pool_config_t config = {
        .max_minicolumns = 0,
        .max_hypercolumns = 10,
        .max_neurons_per_minicolumn = 100,
        .enable_cow_support = false
    };
    pool = cortical_column_pool_create(&config);
    EXPECT_EQ(pool, nullptr);

    cortical_column_pool_config_t config2 = {
        .max_minicolumns = 100,
        .max_hypercolumns = 0,
        .max_neurons_per_minicolumn = 100,
        .enable_cow_support = false
    };
    pool = cortical_column_pool_create(&config2);
    EXPECT_EQ(pool, nullptr);
}

/**
 * WHAT: Test pool destruction
 * WHY:  Verify clean shutdown
 * HOW:  Create and destroy pool
 */
TEST_F(CorticalColumnTest, PoolDestroy_ValidPool_Success) {
    CreateDefaultPool();
    cortical_column_pool_destroy(pool);
    pool = nullptr;  // Prevent double-free in TearDown
}

/**
 * WHAT: Test pool destruction with NULL
 * WHY:  Verify NULL-safe destruction
 * HOW:  Pass NULL to destroy
 */
TEST_F(CorticalColumnTest, PoolDestroy_NullPool_NoError) {
    cortical_column_pool_destroy(nullptr);
    // Should not crash
}

//=============================================================================
// Minicolumn Lifecycle Tests
//=============================================================================

/**
 * WHAT: Test minicolumn creation with valid config
 * WHY:  Verify basic minicolumn creation works
 * HOW:  Create pool, then minicolumn with valid config
 */
TEST_F(CorticalColumnTest, MinicolumnCreate_ValidConfig_Success) {
    CreateDefaultPool();

    std::vector<uint32_t> neuron_ids = {0, 1, 2, 3, 4, 5, 6, 7, 8};
    minicolumn_config_t config = CreateTestMinicolumnConfig(neuron_ids, 45.0f);

    minicolumn_t* col = minicolumn_create(pool, &config);
    ASSERT_NE(col, nullptr);

    minicolumn_destroy(col);
}

/**
 * WHAT: Test minicolumn creation with various neuron counts
 * WHY:  Verify minicolumn handles different sizes
 * HOW:  Create minicolumns with 10, 50, 100 neurons
 */
TEST_F(CorticalColumnTest, MinicolumnCreate_VariousNeuronCounts_Success) {
    CreateDefaultPool();

    // Small minicolumn (10 neurons)
    std::vector<uint32_t> small_ids(10);
    std::iota(small_ids.begin(), small_ids.end(), 0);
    minicolumn_config_t small_config = CreateTestMinicolumnConfig(small_ids, 0.0f);
    minicolumn_t* small_col = minicolumn_create(pool, &small_config);
    ASSERT_NE(small_col, nullptr);
    minicolumn_destroy(small_col);

    // Medium minicolumn (50 neurons)
    std::vector<uint32_t> medium_ids(50);
    std::iota(medium_ids.begin(), medium_ids.end(), 0);
    minicolumn_config_t medium_config = CreateTestMinicolumnConfig(medium_ids, 90.0f);
    minicolumn_t* medium_col = minicolumn_create(pool, &medium_config);
    ASSERT_NE(medium_col, nullptr);
    minicolumn_destroy(medium_col);

    // Large minicolumn (100 neurons)
    std::vector<uint32_t> large_ids(100);
    std::iota(large_ids.begin(), large_ids.end(), 0);
    minicolumn_config_t large_config = CreateTestMinicolumnConfig(large_ids, 180.0f);
    minicolumn_t* large_col = minicolumn_create(pool, &large_config);
    ASSERT_NE(large_col, nullptr);
    minicolumn_destroy(large_col);
}

/**
 * WHAT: Test minicolumn creation with NULL pool
 * WHY:  Verify error handling
 * HOW:  Pass NULL pool, expect NULL return
 */
TEST_F(CorticalColumnTest, MinicolumnCreate_NullPool_ReturnsNull) {
    std::vector<uint32_t> neuron_ids = {0, 1, 2};
    minicolumn_config_t config = CreateTestMinicolumnConfig(neuron_ids, 0.0f);

    minicolumn_t* col = minicolumn_create(nullptr, &config);
    EXPECT_EQ(col, nullptr);
}

/**
 * WHAT: Test minicolumn creation with NULL config
 * WHY:  Verify error handling for invalid input
 * HOW:  Pass NULL config, expect NULL return
 */
TEST_F(CorticalColumnTest, MinicolumnCreate_NullConfig_ReturnsNull) {
    CreateDefaultPool();
    minicolumn_t* col = minicolumn_create(pool, nullptr);
    EXPECT_EQ(col, nullptr);
}

/**
 * WHAT: Test minicolumn creation with zero neurons
 * WHY:  Verify validation of neuron count
 * HOW:  Create config with empty neuron array
 */
TEST_F(CorticalColumnTest, MinicolumnCreate_ZeroNeurons_ReturnsNull) {
    CreateDefaultPool();

    std::vector<uint32_t> neuron_ids;
    minicolumn_config_t config = CreateTestMinicolumnConfig(neuron_ids, 0.0f);

    minicolumn_t* col = minicolumn_create(pool, &config);
    EXPECT_EQ(col, nullptr);
}

/**
 * WHAT: Test minicolumn creation with invalid layer distribution
 * WHY:  Verify validation of layer counts
 * HOW:  Create config where layer sum != num_neurons
 */
TEST_F(CorticalColumnTest, MinicolumnCreate_InvalidLayerDistribution_ReturnsNull) {
    CreateDefaultPool();

    std::vector<uint32_t> neuron_ids = {0, 1, 2, 3, 4, 5, 6, 7, 8};
    minicolumn_config_t config = CreateTestMinicolumnConfig(neuron_ids, 0.0f);

    // Intentionally make layer sum wrong
    config.layers.layer_2_3_count = 5;
    config.layers.layer_4_count = 5;
    config.layers.layer_5_6_count = 5;  // Sum = 15, but num_neurons = 9

    minicolumn_t* col = minicolumn_create(pool, &config);
    EXPECT_EQ(col, nullptr);
}

/**
 * WHAT: Test minicolumn creation with invalid receptive field
 * WHY:  Verify validation of receptive field radius
 * HOW:  Create config with zero/negative radius
 */
TEST_F(CorticalColumnTest, MinicolumnCreate_InvalidReceptiveField_ReturnsNull) {
    CreateDefaultPool();

    std::vector<uint32_t> neuron_ids = {0, 1, 2};
    minicolumn_config_t config = CreateTestMinicolumnConfig(neuron_ids, 0.0f);
    config.receptive_field.radius = 0.0f;  // Invalid

    minicolumn_t* col = minicolumn_create(pool, &config);
    EXPECT_EQ(col, nullptr);

    config.receptive_field.radius = -1.0f;  // Invalid
    col = minicolumn_create(pool, &config);
    EXPECT_EQ(col, nullptr);
}

/**
 * WHAT: Test minicolumn pool exhaustion
 * WHY:  Verify behavior when pool runs out of memory
 * HOW:  Create small pool, allocate until exhausted
 */
TEST_F(CorticalColumnTest, MinicolumnCreate_PoolExhaustion_ReturnsNull) {
    cortical_column_pool_config_t config = {
        .max_minicolumns = 3,  // Very small pool
        .max_hypercolumns = 1,
        .max_neurons_per_minicolumn = 10,
        .enable_cow_support = false
    };
    pool = cortical_column_pool_create(&config);
    ASSERT_NE(pool, nullptr);

    std::vector<minicolumn_t*> cols;
    std::vector<uint32_t> neuron_ids = {0, 1, 2};
    minicolumn_config_t mc_config = CreateTestMinicolumnConfig(neuron_ids, 0.0f);

    // Fill pool
    for (int i = 0; i < 3; i++) {
        minicolumn_t* col = minicolumn_create(pool, &mc_config);
        ASSERT_NE(col, nullptr);
        cols.push_back(col);
    }

    // Pool should be exhausted
    minicolumn_t* extra = minicolumn_create(pool, &mc_config);
    EXPECT_EQ(extra, nullptr);

    // Cleanup
    for (auto col : cols) {
        minicolumn_destroy(col);
    }
}

/**
 * WHAT: Test minicolumn destruction
 * WHY:  Verify clean resource release
 * HOW:  Create and destroy minicolumn
 */
TEST_F(CorticalColumnTest, MinicolumnDestroy_ValidMinicolumn_Success) {
    CreateDefaultPool();

    std::vector<uint32_t> neuron_ids = {0, 1, 2};
    minicolumn_config_t config = CreateTestMinicolumnConfig(neuron_ids, 0.0f);
    minicolumn_t* col = minicolumn_create(pool, &config);
    ASSERT_NE(col, nullptr);

    minicolumn_destroy(col);
    // Should not crash
}

/**
 * WHAT: Test minicolumn destruction with NULL
 * WHY:  Verify NULL-safe destruction
 * HOW:  Pass NULL to destroy
 */
TEST_F(CorticalColumnTest, MinicolumnDestroy_Null_NoError) {
    minicolumn_destroy(nullptr);
    // Should not crash
}

//=============================================================================
// Hypercolumn Lifecycle Tests
//=============================================================================

/**
 * WHAT: Test hypercolumn creation with WINNER_TAKE_ALL mode
 * WHY:  Verify basic hypercolumn creation works
 * HOW:  Create hypercolumn with multiple minicolumns
 */
TEST_F(CorticalColumnTest, HypercolumnCreate_WinnerTakeAll_Success) {
    CreateDefaultPool();

    // Create minicolumn configs
    std::vector<std::vector<uint32_t>> neuron_id_arrays(5);
    std::vector<minicolumn_config_t> mc_configs(5);

    for (int i = 0; i < 5; i++) {
        neuron_id_arrays[i] = {
            static_cast<uint32_t>(i * 10 + 0),
            static_cast<uint32_t>(i * 10 + 1),
            static_cast<uint32_t>(i * 10 + 2)
        };
        mc_configs[i] = CreateTestMinicolumnConfig(neuron_id_arrays[i], i * 36.0f);
    }

    hypercolumn_config_t hc_config = {};
    hc_config.num_minicolumns = 5;
    hc_config.minicolumn_configs = mc_configs.data();
    hc_config.feature_space_min = 0.0f;
    hc_config.feature_space_max = 180.0f;
    hc_config.topographic_x = 0.0f;
    hc_config.topographic_y = 0.0f;
    hc_config.competition = CC_COMPETITION_WINNER_TAKE_ALL;
    hc_config.k_winners = 0;
    hc_config.temperature = 1.0f;
    hc_config.lateral_inhibition_strength = 0.5f;
    hc_config.lateral_inhibition_sigma1 = 1.0f;
    hc_config.lateral_inhibition_sigma2 = 3.0f;

    hypercolumn_t* hcol = hypercolumn_create(pool, &hc_config);
    ASSERT_NE(hcol, nullptr);

    hypercolumn_destroy(hcol);
}

/**
 * WHAT: Test hypercolumn creation with K_WINNERS mode
 * WHY:  Verify K-winners competition mode works
 * HOW:  Create hypercolumn with k_winners = 3
 */
TEST_F(CorticalColumnTest, HypercolumnCreate_KWinners_Success) {
    CreateDefaultPool();

    std::vector<std::vector<uint32_t>> neuron_id_arrays(10);
    std::vector<minicolumn_config_t> mc_configs(10);

    for (int i = 0; i < 10; i++) {
        neuron_id_arrays[i] = {
            static_cast<uint32_t>(i * 5 + 0),
            static_cast<uint32_t>(i * 5 + 1),
            static_cast<uint32_t>(i * 5 + 2)
        };
        mc_configs[i] = CreateTestMinicolumnConfig(neuron_id_arrays[i], i * 18.0f);
    }

    hypercolumn_config_t hc_config = {};
    hc_config.num_minicolumns = 10;
    hc_config.minicolumn_configs = mc_configs.data();
    hc_config.feature_space_min = 0.0f;
    hc_config.feature_space_max = 180.0f;
    hc_config.topographic_x = 1.0f;
    hc_config.topographic_y = 1.0f;
    hc_config.competition = CC_COMPETITION_K_WINNERS;
    hc_config.k_winners = 3;
    hc_config.temperature = 1.0f;
    hc_config.lateral_inhibition_strength = 0.3f;
    hc_config.lateral_inhibition_sigma1 = 1.5f;
    hc_config.lateral_inhibition_sigma2 = 4.0f;

    hypercolumn_t* hcol = hypercolumn_create(pool, &hc_config);
    ASSERT_NE(hcol, nullptr);

    hypercolumn_destroy(hcol);
}

/**
 * WHAT: Test hypercolumn creation with SOFTMAX mode
 * WHY:  Verify softmax competition mode works
 * HOW:  Create hypercolumn with temperature parameter
 */
TEST_F(CorticalColumnTest, HypercolumnCreate_Softmax_Success) {
    CreateDefaultPool();

    std::vector<std::vector<uint32_t>> neuron_id_arrays(8);
    std::vector<minicolumn_config_t> mc_configs(8);

    for (int i = 0; i < 8; i++) {
        neuron_id_arrays[i] = {
            static_cast<uint32_t>(i * 4 + 0),
            static_cast<uint32_t>(i * 4 + 1),
            static_cast<uint32_t>(i * 4 + 2),
            static_cast<uint32_t>(i * 4 + 3)
        };
        mc_configs[i] = CreateTestMinicolumnConfig(neuron_id_arrays[i], i * 22.5f);
    }

    hypercolumn_config_t hc_config = {};
    hc_config.num_minicolumns = 8;
    hc_config.minicolumn_configs = mc_configs.data();
    hc_config.feature_space_min = 0.0f;
    hc_config.feature_space_max = 180.0f;
    hc_config.topographic_x = 2.0f;
    hc_config.topographic_y = 2.0f;
    hc_config.competition = CC_COMPETITION_SOFTMAX;
    hc_config.k_winners = 0;
    hc_config.temperature = 0.5f;  // Low temperature for sharper distribution
    hc_config.lateral_inhibition_strength = 0.4f;
    hc_config.lateral_inhibition_sigma1 = 1.2f;
    hc_config.lateral_inhibition_sigma2 = 3.5f;

    hypercolumn_t* hcol = hypercolumn_create(pool, &hc_config);
    ASSERT_NE(hcol, nullptr);

    hypercolumn_destroy(hcol);
}

/**
 * WHAT: Test hypercolumn creation with CC_COMPETITION_NONE mode
 * WHY:  Verify no-competition mode works
 * HOW:  Create hypercolumn with competition = NONE
 */
TEST_F(CorticalColumnTest, HypercolumnCreate_NoCompetition_Success) {
    CreateDefaultPool();

    std::vector<std::vector<uint32_t>> neuron_id_arrays(4);
    std::vector<minicolumn_config_t> mc_configs(4);

    for (int i = 0; i < 4; i++) {
        neuron_id_arrays[i] = {
            static_cast<uint32_t>(i * 3 + 0),
            static_cast<uint32_t>(i * 3 + 1),
            static_cast<uint32_t>(i * 3 + 2)
        };
        mc_configs[i] = CreateTestMinicolumnConfig(neuron_id_arrays[i], i * 45.0f);
    }

    hypercolumn_config_t hc_config = {};
    hc_config.num_minicolumns = 4;
    hc_config.minicolumn_configs = mc_configs.data();
    hc_config.feature_space_min = 0.0f;
    hc_config.feature_space_max = 180.0f;
    hc_config.topographic_x = 0.0f;
    hc_config.topographic_y = 0.0f;
    hc_config.competition = CC_COMPETITION_NONE;
    hc_config.k_winners = 0;
    hc_config.temperature = 1.0f;
    hc_config.lateral_inhibition_strength = 0.2f;
    hc_config.lateral_inhibition_sigma1 = 1.0f;
    hc_config.lateral_inhibition_sigma2 = 2.5f;

    hypercolumn_t* hcol = hypercolumn_create(pool, &hc_config);
    ASSERT_NE(hcol, nullptr);

    hypercolumn_destroy(hcol);
}

/**
 * WHAT: Test hypercolumn creation with NULL pool
 * WHY:  Verify error handling
 * HOW:  Pass NULL pool, expect NULL return
 */
TEST_F(CorticalColumnTest, HypercolumnCreate_NullPool_ReturnsNull) {
    std::vector<uint32_t> neuron_ids = {0, 1, 2};
    minicolumn_config_t mc_config = CreateTestMinicolumnConfig(neuron_ids, 0.0f);

    hypercolumn_config_t hc_config = {};
    hc_config.num_minicolumns = 1;
    hc_config.minicolumn_configs = &mc_config;
    hc_config.feature_space_min = 0.0f;
    hc_config.feature_space_max = 180.0f;
    hc_config.competition = CC_COMPETITION_WINNER_TAKE_ALL;

    hypercolumn_t* hcol = hypercolumn_create(nullptr, &hc_config);
    EXPECT_EQ(hcol, nullptr);
}

/**
 * WHAT: Test hypercolumn creation with NULL config
 * WHY:  Verify error handling
 * HOW:  Pass NULL config, expect NULL return
 */
TEST_F(CorticalColumnTest, HypercolumnCreate_NullConfig_ReturnsNull) {
    CreateDefaultPool();
    hypercolumn_t* hcol = hypercolumn_create(pool, nullptr);
    EXPECT_EQ(hcol, nullptr);
}

/**
 * WHAT: Test hypercolumn creation with zero minicolumns
 * WHY:  Verify validation of minicolumn count
 * HOW:  Create config with num_minicolumns = 0
 */
TEST_F(CorticalColumnTest, HypercolumnCreate_ZeroMinicolumns_ReturnsNull) {
    CreateDefaultPool();

    hypercolumn_config_t hc_config = {};
    hc_config.num_minicolumns = 0;
    hc_config.minicolumn_configs = nullptr;
    hc_config.feature_space_min = 0.0f;
    hc_config.feature_space_max = 180.0f;
    hc_config.competition = CC_COMPETITION_WINNER_TAKE_ALL;

    hypercolumn_t* hcol = hypercolumn_create(pool, &hc_config);
    EXPECT_EQ(hcol, nullptr);
}

/**
 * WHAT: Test hypercolumn creation with invalid feature space
 * WHY:  Verify validation of feature space range
 * HOW:  Create config with min >= max
 */
TEST_F(CorticalColumnTest, HypercolumnCreate_InvalidFeatureSpace_ReturnsNull) {
    CreateDefaultPool();

    std::vector<uint32_t> neuron_ids = {0, 1, 2};
    minicolumn_config_t mc_config = CreateTestMinicolumnConfig(neuron_ids, 0.0f);

    hypercolumn_config_t hc_config = {};
    hc_config.num_minicolumns = 1;
    hc_config.minicolumn_configs = &mc_config;
    hc_config.feature_space_min = 180.0f;
    hc_config.feature_space_max = 0.0f;  // min > max
    hc_config.competition = CC_COMPETITION_WINNER_TAKE_ALL;

    hypercolumn_t* hcol = hypercolumn_create(pool, &hc_config);
    EXPECT_EQ(hcol, nullptr);
}

/**
 * WHAT: Test hypercolumn creation with K_WINNERS but k=0
 * WHY:  Verify validation of k_winners parameter
 * HOW:  Create config with K_WINNERS mode but k_winners = 0
 */
TEST_F(CorticalColumnTest, HypercolumnCreate_KWinnersWithZeroK_ReturnsNull) {
    CreateDefaultPool();

    std::vector<uint32_t> neuron_ids = {0, 1, 2};
    minicolumn_config_t mc_config = CreateTestMinicolumnConfig(neuron_ids, 0.0f);

    hypercolumn_config_t hc_config = {};
    hc_config.num_minicolumns = 1;
    hc_config.minicolumn_configs = &mc_config;
    hc_config.feature_space_min = 0.0f;
    hc_config.feature_space_max = 180.0f;
    hc_config.competition = CC_COMPETITION_K_WINNERS;
    hc_config.k_winners = 0;  // Invalid for K_WINNERS mode

    hypercolumn_t* hcol = hypercolumn_create(pool, &hc_config);
    EXPECT_EQ(hcol, nullptr);
}

/**
 * WHAT: Test hypercolumn destruction
 * WHY:  Verify clean resource release
 * HOW:  Create and destroy hypercolumn
 */
TEST_F(CorticalColumnTest, HypercolumnDestroy_ValidHypercolumn_Success) {
    CreateDefaultPool();

    std::vector<uint32_t> neuron_ids = {0, 1, 2};
    minicolumn_config_t mc_config = CreateTestMinicolumnConfig(neuron_ids, 0.0f);

    hypercolumn_config_t hc_config = {};
    hc_config.num_minicolumns = 1;
    hc_config.minicolumn_configs = &mc_config;
    hc_config.feature_space_min = 0.0f;
    hc_config.feature_space_max = 180.0f;
    hc_config.competition = CC_COMPETITION_WINNER_TAKE_ALL;

    hypercolumn_t* hcol = hypercolumn_create(pool, &hc_config);
    ASSERT_NE(hcol, nullptr);

    hypercolumn_destroy(hcol);
    // Should not crash
}

/**
 * WHAT: Test hypercolumn destruction with NULL
 * WHY:  Verify NULL-safe destruction
 * HOW:  Pass NULL to destroy
 */
TEST_F(CorticalColumnTest, HypercolumnDestroy_Null_NoError) {
    hypercolumn_destroy(nullptr);
    // Should not crash
}

//=============================================================================
// Minicolumn Computation Tests
//=============================================================================

/**
 * WHAT: Test minicolumn compute with valid input
 * WHY:  Verify activation computation works
 * HOW:  Create minicolumn, compute with test input
 */
TEST_F(CorticalColumnTest, MinicolumnCompute_ValidInput_ReturnsActivation) {
    CreateDefaultPool();

    std::vector<uint32_t> neuron_ids = {0, 1, 2, 3, 4, 5, 6, 7, 8};
    minicolumn_config_t config = CreateTestMinicolumnConfig(neuron_ids, 45.0f);
    config.receptive_field.center_x = 0.5f;
    config.receptive_field.center_y = 0.5f;
    config.receptive_field.center_z = 0.5f;
    config.receptive_field.radius = 1.0f;

    minicolumn_t* col = minicolumn_create(pool, &config);
    ASSERT_NE(col, nullptr);

    float input[3] = {0.5f, 0.5f, 0.5f};  // At receptive field center
    float activation = minicolumn_compute(col, input, 3);

    EXPECT_GE(activation, 0.0f);
    EXPECT_LE(activation, 1.0f);

    // Should be high activation (at center)
    EXPECT_GT(activation, 0.9f);

    minicolumn_destroy(col);
}

/**
 * WHAT: Test minicolumn compute with input at distance
 * WHY:  Verify Gaussian falloff with distance
 * HOW:  Compute activations at various distances, verify Gaussian formula
 */
TEST_F(CorticalColumnTest, MinicolumnCompute_DistanceDecay_FollowsGaussian) {
    CreateDefaultPool();

    std::vector<uint32_t> neuron_ids = {0, 1, 2};
    minicolumn_config_t config = CreateTestMinicolumnConfig(neuron_ids, 0.0f);
    config.receptive_field.center_x = 0.0f;
    config.receptive_field.center_y = 0.0f;
    config.receptive_field.center_z = 0.0f;
    config.receptive_field.radius = 1.0f;

    minicolumn_t* col = minicolumn_create(pool, &config);
    ASSERT_NE(col, nullptr);

    // Test at center (distance = 0)
    float input1[3] = {0.0f, 0.0f, 0.0f};
    float act1 = minicolumn_compute(col, input1, 3);
    EXPECT_NEAR(act1, 1.0f, TOLERANCE);

    // Test at distance = 1 (1 sigma away)
    float input2[3] = {1.0f, 0.0f, 0.0f};
    float act2 = minicolumn_compute(col, input2, 3);
    float expected2 = ComputeExpectedGaussianWeight(1.0f, 1.0f);
    EXPECT_NEAR(act2, expected2, TOLERANCE);

    // Test at distance = 2 (2 sigma away)
    float input3[3] = {2.0f, 0.0f, 0.0f};
    float act3 = minicolumn_compute(col, input3, 3);
    float expected3 = ComputeExpectedGaussianWeight(2.0f, 1.0f);
    EXPECT_NEAR(act3, expected3, TOLERANCE);

    // Verify decay: act1 > act2 > act3
    EXPECT_GT(act1, act2);
    EXPECT_GT(act2, act3);

    minicolumn_destroy(col);
}

/**
 * WHAT: Test minicolumn compute with NULL minicolumn
 * WHY:  Verify error handling
 * HOW:  Pass NULL, expect error return value
 */
TEST_F(CorticalColumnTest, MinicolumnCompute_NullMinicolumn_ReturnsError) {
    float input[3] = {0.0f, 0.0f, 0.0f};
    float activation = minicolumn_compute(nullptr, input, 3);
    EXPECT_LT(activation, 0.0f);  // Error indicator
}

/**
 * WHAT: Test minicolumn compute with NULL input
 * WHY:  Verify error handling for invalid input
 * HOW:  Pass NULL input, expect error return
 */
TEST_F(CorticalColumnTest, MinicolumnCompute_NullInput_ReturnsError) {
    CreateDefaultPool();

    std::vector<uint32_t> neuron_ids = {0, 1, 2};
    minicolumn_config_t config = CreateTestMinicolumnConfig(neuron_ids, 0.0f);
    minicolumn_t* col = minicolumn_create(pool, &config);
    ASSERT_NE(col, nullptr);

    float activation = minicolumn_compute(col, nullptr, 3);
    EXPECT_LT(activation, 0.0f);

    minicolumn_destroy(col);
}

/**
 * WHAT: Test minicolumn compute with zero input size
 * WHY:  Verify validation of input size
 * HOW:  Pass size = 0, expect error return
 */
TEST_F(CorticalColumnTest, MinicolumnCompute_ZeroInputSize_ReturnsError) {
    CreateDefaultPool();

    std::vector<uint32_t> neuron_ids = {0, 1, 2};
    minicolumn_config_t config = CreateTestMinicolumnConfig(neuron_ids, 0.0f);
    minicolumn_t* col = minicolumn_create(pool, &config);
    ASSERT_NE(col, nullptr);

    float input[3] = {0.0f, 0.0f, 0.0f};
    float activation = minicolumn_compute(col, input, 0);
    EXPECT_LT(activation, 0.0f);

    minicolumn_destroy(col);
}

//=============================================================================
// Hypercolumn Computation Tests
//=============================================================================

/**
 * WHAT: Test hypercolumn compute with winner-take-all
 * WHY:  Verify competition results in single winner
 * HOW:  Create hypercolumn, compute, verify only one active
 */
TEST_F(CorticalColumnTest, HypercolumnCompute_WinnerTakeAll_OnlyOneActive) {
    CreateDefaultPool();

    // Create 5 minicolumns with different tuning preferences
    std::vector<std::vector<uint32_t>> neuron_id_arrays(5);
    std::vector<minicolumn_config_t> mc_configs(5);

    for (int i = 0; i < 5; i++) {
        neuron_id_arrays[i] = {
            static_cast<uint32_t>(i * 3 + 0),
            static_cast<uint32_t>(i * 3 + 1),
            static_cast<uint32_t>(i * 3 + 2)
        };
        mc_configs[i] = CreateTestMinicolumnConfig(neuron_id_arrays[i], i * 36.0f);
        // Different receptive field centers
        mc_configs[i].receptive_field.center_x = i * 0.25f;
        mc_configs[i].receptive_field.center_y = 0.0f;
        mc_configs[i].receptive_field.center_z = 0.0f;
        mc_configs[i].receptive_field.radius = 1.0f;
    }

    hypercolumn_config_t hc_config = {};
    hc_config.num_minicolumns = 5;
    hc_config.minicolumn_configs = mc_configs.data();
    hc_config.feature_space_min = 0.0f;
    hc_config.feature_space_max = 180.0f;
    hc_config.competition = CC_COMPETITION_WINNER_TAKE_ALL;
    hc_config.temperature = 1.0f;

    hypercolumn_t* hcol = hypercolumn_create(pool, &hc_config);
    ASSERT_NE(hcol, nullptr);

    // Input near minicolumn 2's receptive field
    float input[3] = {0.5f, 0.0f, 0.0f};
    hypercolumn_compute(hcol, input, 3);

    // Get distribution
    float distribution[5] = {0.0f};
    hypercolumn_get_distribution(hcol, distribution, 5);

    // Count active minicolumns
    int active_count = 0;
    int winner_idx = -1;
    for (int i = 0; i < 5; i++) {
        if (distribution[i] > 0.5f) {  // Consider active if > 0.5
            active_count++;
            winner_idx = i;
        }
    }

    // Should have exactly 1 winner
    EXPECT_EQ(active_count, 1);
    EXPECT_GE(winner_idx, 0);
    EXPECT_LT(winner_idx, 5);

    // Winner should have activation = 1.0
    if (winner_idx >= 0) {
        EXPECT_NEAR(distribution[winner_idx], 1.0f, TOLERANCE);
    }

    hypercolumn_destroy(hcol);
}

/**
 * WHAT: Test hypercolumn compute with K-winners
 * WHY:  Verify exactly K minicolumns are active
 * HOW:  Create hypercolumn with K=3, verify 3 winners
 */
TEST_F(CorticalColumnTest, HypercolumnCompute_KWinners_ExactlyKActive) {
    CreateDefaultPool();

    // Create 10 minicolumns
    std::vector<std::vector<uint32_t>> neuron_id_arrays(10);
    std::vector<minicolumn_config_t> mc_configs(10);

    for (int i = 0; i < 10; i++) {
        neuron_id_arrays[i] = {
            static_cast<uint32_t>(i * 2 + 0),
            static_cast<uint32_t>(i * 2 + 1)
        };
        mc_configs[i] = CreateTestMinicolumnConfig(neuron_id_arrays[i], i * 18.0f);
        mc_configs[i].receptive_field.center_x = i * 0.1f;
        mc_configs[i].receptive_field.radius = 1.0f;
    }

    hypercolumn_config_t hc_config = {};
    hc_config.num_minicolumns = 10;
    hc_config.minicolumn_configs = mc_configs.data();
    hc_config.feature_space_min = 0.0f;
    hc_config.feature_space_max = 180.0f;
    hc_config.competition = CC_COMPETITION_K_WINNERS;
    hc_config.k_winners = 3;
    hc_config.temperature = 1.0f;

    hypercolumn_t* hcol = hypercolumn_create(pool, &hc_config);
    ASSERT_NE(hcol, nullptr);

    float input[3] = {0.5f, 0.0f, 0.0f};
    hypercolumn_compute(hcol, input, 3);

    float distribution[10] = {0.0f};
    hypercolumn_get_distribution(hcol, distribution, 10);

    // Count active minicolumns
    int active_count = 0;
    for (int i = 0; i < 10; i++) {
        if (distribution[i] > 0.5f) {
            active_count++;
        }
    }

    EXPECT_EQ(active_count, 3);

    hypercolumn_destroy(hcol);
}

/**
 * WHAT: Test hypercolumn compute with softmax
 * WHY:  Verify softmax produces valid probability distribution
 * HOW:  Compute, verify sum = 1.0 and all values in [0,1]
 */
TEST_F(CorticalColumnTest, HypercolumnCompute_Softmax_ValidDistribution) {
    CreateDefaultPool();

    // Create 8 minicolumns
    std::vector<std::vector<uint32_t>> neuron_id_arrays(8);
    std::vector<minicolumn_config_t> mc_configs(8);

    for (int i = 0; i < 8; i++) {
        neuron_id_arrays[i] = {
            static_cast<uint32_t>(i * 3 + 0),
            static_cast<uint32_t>(i * 3 + 1),
            static_cast<uint32_t>(i * 3 + 2)
        };
        mc_configs[i] = CreateTestMinicolumnConfig(neuron_id_arrays[i], i * 22.5f);
        mc_configs[i].receptive_field.center_x = i * 0.125f;
        mc_configs[i].receptive_field.radius = 0.5f;
    }

    hypercolumn_config_t hc_config = {};
    hc_config.num_minicolumns = 8;
    hc_config.minicolumn_configs = mc_configs.data();
    hc_config.feature_space_min = 0.0f;
    hc_config.feature_space_max = 180.0f;
    hc_config.competition = CC_COMPETITION_SOFTMAX;
    hc_config.temperature = 1.0f;

    hypercolumn_t* hcol = hypercolumn_create(pool, &hc_config);
    ASSERT_NE(hcol, nullptr);

    float input[3] = {0.5f, 0.0f, 0.0f};
    hypercolumn_compute(hcol, input, 3);

    float distribution[8] = {0.0f};
    hypercolumn_get_distribution(hcol, distribution, 8);

    // Verify all values in [0, 1]
    for (int i = 0; i < 8; i++) {
        EXPECT_GE(distribution[i], 0.0f);
        EXPECT_LE(distribution[i], 1.0f);
    }

    // Verify sum approximately equals 1.0
    float sum = 0.0f;
    for (int i = 0; i < 8; i++) {
        sum += distribution[i];
    }
    EXPECT_NEAR(sum, 1.0f, TOLERANCE);

    hypercolumn_destroy(hcol);
}

/**
 * WHAT: Test hypercolumn compute with no competition
 * WHY:  Verify activations preserved without competition
 * HOW:  Compute with CC_COMPETITION_NONE, verify multiple active
 */
TEST_F(CorticalColumnTest, HypercolumnCompute_NoCompetition_MultipleActive) {
    CreateDefaultPool();

    std::vector<std::vector<uint32_t>> neuron_id_arrays(5);
    std::vector<minicolumn_config_t> mc_configs(5);

    for (int i = 0; i < 5; i++) {
        neuron_id_arrays[i] = {
            static_cast<uint32_t>(i * 2 + 0),
            static_cast<uint32_t>(i * 2 + 1)
        };
        mc_configs[i] = CreateTestMinicolumnConfig(neuron_id_arrays[i], i * 36.0f);
        mc_configs[i].receptive_field.center_x = 0.5f;  // All have same center
        mc_configs[i].receptive_field.radius = 1.0f;
    }

    hypercolumn_config_t hc_config = {};
    hc_config.num_minicolumns = 5;
    hc_config.minicolumn_configs = mc_configs.data();
    hc_config.feature_space_min = 0.0f;
    hc_config.feature_space_max = 180.0f;
    hc_config.competition = CC_COMPETITION_NONE;

    hypercolumn_t* hcol = hypercolumn_create(pool, &hc_config);
    ASSERT_NE(hcol, nullptr);

    float input[3] = {0.5f, 0.0f, 0.0f};
    hypercolumn_compute(hcol, input, 3);

    float distribution[5] = {0.0f};
    hypercolumn_get_distribution(hcol, distribution, 5);

    // With no competition, multiple should be active
    int active_count = 0;
    for (int i = 0; i < 5; i++) {
        if (distribution[i] > 0.1f) {  // Low threshold
            active_count++;
        }
    }

    EXPECT_GT(active_count, 1);  // More than 1 active

    hypercolumn_destroy(hcol);
}

/**
 * WHAT: Test hypercolumn get_winner function
 * WHY:  Verify correct winner is identified
 * HOW:  Compute, check winner index matches highest activation
 */
TEST_F(CorticalColumnTest, HypercolumnGetWinner_AfterCompute_CorrectIndex) {
    CreateDefaultPool();

    std::vector<std::vector<uint32_t>> neuron_id_arrays(5);
    std::vector<minicolumn_config_t> mc_configs(5);

    for (int i = 0; i < 5; i++) {
        neuron_id_arrays[i] = {
            static_cast<uint32_t>(i * 3 + 0),
            static_cast<uint32_t>(i * 3 + 1),
            static_cast<uint32_t>(i * 3 + 2)
        };
        mc_configs[i] = CreateTestMinicolumnConfig(neuron_id_arrays[i], i * 36.0f);
        mc_configs[i].receptive_field.center_x = i * 0.25f;
        mc_configs[i].receptive_field.radius = 1.0f;
    }

    hypercolumn_config_t hc_config = {};
    hc_config.num_minicolumns = 5;
    hc_config.minicolumn_configs = mc_configs.data();
    hc_config.feature_space_min = 0.0f;
    hc_config.feature_space_max = 180.0f;
    hc_config.competition = CC_COMPETITION_WINNER_TAKE_ALL;

    hypercolumn_t* hcol = hypercolumn_create(pool, &hc_config);
    ASSERT_NE(hcol, nullptr);

    // Input closest to minicolumn 3
    float input[3] = {0.75f, 0.0f, 0.0f};
    hypercolumn_compute(hcol, input, 3);

    uint32_t winner = hypercolumn_get_winner(hcol);
    EXPECT_LT(winner, 5);

    // Verify winner has highest activation
    float distribution[5] = {0.0f};
    hypercolumn_get_distribution(hcol, distribution, 5);

    for (uint32_t i = 0; i < 5; i++) {
        if (i != winner) {
            EXPECT_LE(distribution[i], distribution[winner]);
        }
    }

    hypercolumn_destroy(hcol);
}

/**
 * WHAT: Test hypercolumn get_winner with NULL hypercolumn
 * WHY:  Verify error handling
 * HOW:  Pass NULL, expect error return value
 */
TEST_F(CorticalColumnTest, HypercolumnGetWinner_Null_ReturnsError) {
    uint32_t winner = hypercolumn_get_winner(nullptr);
    EXPECT_EQ(winner, UINT32_MAX);
}

/**
 * WHAT: Test hypercolumn get_distribution with NULL hypercolumn
 * WHY:  Verify error handling
 * HOW:  Pass NULL, should not crash
 */
TEST_F(CorticalColumnTest, HypercolumnGetDistribution_NullHypercolumn_NoError) {
    float distribution[5] = {0.0f};
    hypercolumn_get_distribution(nullptr, distribution, 5);
    // Should not crash
}

/**
 * WHAT: Test hypercolumn get_distribution with NULL output
 * WHY:  Verify error handling
 * HOW:  Pass NULL output, should not crash
 */
TEST_F(CorticalColumnTest, HypercolumnGetDistribution_NullOutput_NoError) {
    CreateDefaultPool();

    std::vector<uint32_t> neuron_ids = {0, 1, 2};
    minicolumn_config_t mc_config = CreateTestMinicolumnConfig(neuron_ids, 0.0f);

    hypercolumn_config_t hc_config = {};
    hc_config.num_minicolumns = 1;
    hc_config.minicolumn_configs = &mc_config;
    hc_config.feature_space_min = 0.0f;
    hc_config.feature_space_max = 180.0f;
    hc_config.competition = CC_COMPETITION_WINNER_TAKE_ALL;

    hypercolumn_t* hcol = hypercolumn_create(pool, &hc_config);
    ASSERT_NE(hcol, nullptr);

    hypercolumn_get_distribution(hcol, nullptr, 1);
    // Should not crash

    hypercolumn_destroy(hcol);
}

/**
 * WHAT: Test hypercolumn get_distribution with insufficient buffer size
 * WHY:  Verify error handling for buffer overflow
 * HOW:  Pass buffer smaller than num_minicolumns
 */
TEST_F(CorticalColumnTest, HypercolumnGetDistribution_SmallBuffer_NoError) {
    CreateDefaultPool();

    std::vector<std::vector<uint32_t>> neuron_id_arrays(5);
    std::vector<minicolumn_config_t> mc_configs(5);

    for (int i = 0; i < 5; i++) {
        neuron_id_arrays[i] = {static_cast<uint32_t>(i)};
        mc_configs[i] = CreateTestMinicolumnConfig(neuron_id_arrays[i], 0.0f);
    }

    hypercolumn_config_t hc_config = {};
    hc_config.num_minicolumns = 5;
    hc_config.minicolumn_configs = mc_configs.data();
    hc_config.feature_space_min = 0.0f;
    hc_config.feature_space_max = 180.0f;
    hc_config.competition = CC_COMPETITION_WINNER_TAKE_ALL;

    hypercolumn_t* hcol = hypercolumn_create(pool, &hc_config);
    ASSERT_NE(hcol, nullptr);

    float distribution[3] = {0.0f};  // Too small
    hypercolumn_get_distribution(hcol, distribution, 3);
    // Should not crash (function should detect and not write)

    hypercolumn_destroy(hcol);
}

//=============================================================================
// Lateral Inhibition Tests
//=============================================================================

/**
 * WHAT: Test lateral inhibition application
 * WHY:  Verify inhibition reduces activation
 * HOW:  Apply inhibition, verify clamping to [0, 1]
 */
TEST_F(CorticalColumnTest, LateralInhibition_Apply_ReducesActivation) {
    CreateDefaultPool();

    std::vector<uint32_t> neuron_ids = {0, 1, 2};
    minicolumn_config_t config = CreateTestMinicolumnConfig(neuron_ids, 0.0f);
    minicolumn_t* col = minicolumn_create(pool, &config);
    ASSERT_NE(col, nullptr);

    // First compute to set activation
    float input[3] = {0.0f, 0.0f, 0.0f};
    float initial_activation = minicolumn_compute(col, input, 3);
    EXPECT_GT(initial_activation, 0.0f);

    // Apply inhibition
    minicolumn_apply_lateral_inhibition(col, 0.3f);

    // Get stats to check inhibition level
    minicolumn_stats_t stats;
    minicolumn_get_stats(col, &stats);
    EXPECT_NEAR(stats.inhibition_level, 0.3f, TOLERANCE);

    minicolumn_destroy(col);
}

/**
 * WHAT: Test lateral inhibition clamping
 * WHY:  Verify inhibition is clamped to [0, 1]
 * HOW:  Apply excessive inhibition, verify clamped
 */
TEST_F(CorticalColumnTest, LateralInhibition_ExcessiveValue_ClampedToOne) {
    CreateDefaultPool();

    std::vector<uint32_t> neuron_ids = {0, 1, 2};
    minicolumn_config_t config = CreateTestMinicolumnConfig(neuron_ids, 0.0f);
    minicolumn_t* col = minicolumn_create(pool, &config);
    ASSERT_NE(col, nullptr);

    // Apply excessive inhibition
    minicolumn_apply_lateral_inhibition(col, 5.0f);

    minicolumn_stats_t stats;
    minicolumn_get_stats(col, &stats);
    EXPECT_LE(stats.inhibition_level, 1.0f);

    minicolumn_destroy(col);
}

/**
 * WHAT: Test lateral inhibition with negative value
 * WHY:  Verify negative inhibition is clamped to 0
 * HOW:  Apply negative inhibition, verify clamped
 */
TEST_F(CorticalColumnTest, LateralInhibition_NegativeValue_ClampedToZero) {
    CreateDefaultPool();

    std::vector<uint32_t> neuron_ids = {0, 1, 2};
    minicolumn_config_t config = CreateTestMinicolumnConfig(neuron_ids, 0.0f);
    minicolumn_t* col = minicolumn_create(pool, &config);
    ASSERT_NE(col, nullptr);

    minicolumn_apply_lateral_inhibition(col, -0.5f);

    minicolumn_stats_t stats;
    minicolumn_get_stats(col, &stats);
    EXPECT_GE(stats.inhibition_level, 0.0f);

    minicolumn_destroy(col);
}

/**
 * WHAT: Test lateral inhibition with NULL minicolumn
 * WHY:  Verify error handling
 * HOW:  Pass NULL, should not crash
 */
TEST_F(CorticalColumnTest, LateralInhibition_Null_NoError) {
    minicolumn_apply_lateral_inhibition(nullptr, 0.5f);
    // Should not crash
}

//=============================================================================
// Competition Mode Tests
//=============================================================================

/**
 * WHAT: Test winner-take-all competition directly
 * WHY:  Verify WTA algorithm correctness
 * HOW:  Run competition, verify only max is 1.0
 */
TEST_F(CorticalColumnTest, Competition_WinnerTakeAll_OnlyMaxActive) {
    CreateDefaultPool();

    std::vector<std::vector<uint32_t>> neuron_id_arrays(5);
    std::vector<minicolumn_config_t> mc_configs(5);

    for (int i = 0; i < 5; i++) {
        neuron_id_arrays[i] = {static_cast<uint32_t>(i)};
        mc_configs[i] = CreateTestMinicolumnConfig(neuron_id_arrays[i], 0.0f);
    }

    hypercolumn_config_t hc_config = {};
    hc_config.num_minicolumns = 5;
    hc_config.minicolumn_configs = mc_configs.data();
    hc_config.feature_space_min = 0.0f;
    hc_config.feature_space_max = 180.0f;
    hc_config.competition = CC_COMPETITION_NONE;  // Start with no competition

    hypercolumn_t* hcol = hypercolumn_create(pool, &hc_config);
    ASSERT_NE(hcol, nullptr);

    // Set up initial activations (via compute)
    float input[3] = {0.5f, 0.0f, 0.0f};
    hypercolumn_compute(hcol, input, 3);

    // Now apply WTA
    hypercolumn_run_competition(hcol, CC_COMPETITION_WINNER_TAKE_ALL, 1.0f);

    float distribution[5] = {0.0f};
    hypercolumn_get_distribution(hcol, distribution, 5);

    // Count values = 1.0
    int ones_count = 0;
    int zeros_count = 0;
    for (int i = 0; i < 5; i++) {
        if (std::abs(distribution[i] - 1.0f) < TOLERANCE) {
            ones_count++;
        } else if (std::abs(distribution[i]) < TOLERANCE) {
            zeros_count++;
        }
    }

    EXPECT_EQ(ones_count, 1);
    EXPECT_EQ(zeros_count, 4);

    hypercolumn_destroy(hcol);
}

/**
 * WHAT: Test softmax competition with different temperatures
 * WHY:  Verify temperature affects sharpness of distribution
 * HOW:  Run softmax with T=0.1 and T=10.0, compare entropy
 */
TEST_F(CorticalColumnTest, Competition_Softmax_TemperatureAffectsSharpness) {
    CreateDefaultPool();

    // Create hypercolumn
    std::vector<std::vector<uint32_t>> neuron_id_arrays(5);
    std::vector<minicolumn_config_t> mc_configs(5);

    for (int i = 0; i < 5; i++) {
        neuron_id_arrays[i] = {static_cast<uint32_t>(i)};
        mc_configs[i] = CreateTestMinicolumnConfig(neuron_id_arrays[i], i * 36.0f);
        mc_configs[i].receptive_field.center_x = i * 0.2f;
        mc_configs[i].receptive_field.radius = 1.0f;
    }

    // Test low temperature (sharp)
    {
        hypercolumn_config_t hc_config = {};
        hc_config.num_minicolumns = 5;
        hc_config.minicolumn_configs = mc_configs.data();
        hc_config.feature_space_min = 0.0f;
        hc_config.feature_space_max = 180.0f;
        hc_config.competition = CC_COMPETITION_SOFTMAX;
        hc_config.temperature = 0.1f;  // Low temperature

        hypercolumn_t* hcol = hypercolumn_create(pool, &hc_config);
        ASSERT_NE(hcol, nullptr);

        float input[3] = {0.5f, 0.0f, 0.0f};
        hypercolumn_compute(hcol, input, 3);

        cc_hypercolumn_stats_t stats;
        hypercolumn_get_stats(hcol, &stats);
        float low_temp_entropy = stats.entropy;

        hypercolumn_destroy(hcol);

        // High temperature (flat)
        hc_config.temperature = 10.0f;
        hcol = hypercolumn_create(pool, &hc_config);
        ASSERT_NE(hcol, nullptr);

        hypercolumn_compute(hcol, input, 3);
        hypercolumn_get_stats(hcol, &stats);
        float high_temp_entropy = stats.entropy;

        // High temperature should have higher entropy (more uniform)
        EXPECT_LT(low_temp_entropy, high_temp_entropy);

        hypercolumn_destroy(hcol);
    }
}

//=============================================================================
// Receptive Field Tests
//=============================================================================

/**
 * WHAT: Test receptive field setter
 * WHY:  Verify receptive field can be updated
 * HOW:  Create minicolumn, update RF, verify with compute_receptive_weight
 */
TEST_F(CorticalColumnTest, ReceptiveField_Set_UpdatesCorrectly) {
    CreateDefaultPool();

    std::vector<uint32_t> neuron_ids = {0, 1, 2};
    minicolumn_config_t config = CreateTestMinicolumnConfig(neuron_ids, 0.0f);
    config.receptive_field.center_x = 0.0f;
    config.receptive_field.center_y = 0.0f;
    config.receptive_field.center_z = 0.0f;
    config.receptive_field.radius = 1.0f;

    minicolumn_t* col = minicolumn_create(pool, &config);
    ASSERT_NE(col, nullptr);

    // Update receptive field
    minicolumn_set_receptive_field(col, 1.0f, 2.0f, 3.0f, 2.0f);

    // Verify by computing weight at new center
    float weight = minicolumn_compute_receptive_weight(col, 1.0f, 2.0f, 3.0f);
    EXPECT_NEAR(weight, 1.0f, TOLERANCE);  // At center, weight should be 1.0

    minicolumn_destroy(col);
}

/**
 * WHAT: Test receptive field weight computation
 * WHY:  Verify Gaussian formula: exp(-d²/2σ²)
 * HOW:  Compute weights at various distances, compare with formula
 */
TEST_F(CorticalColumnTest, ReceptiveField_ComputeWeight_FollowsGaussian) {
    CreateDefaultPool();

    std::vector<uint32_t> neuron_ids = {0, 1, 2};
    minicolumn_config_t config = CreateTestMinicolumnConfig(neuron_ids, 0.0f);
    config.receptive_field.center_x = 0.0f;
    config.receptive_field.center_y = 0.0f;
    config.receptive_field.center_z = 0.0f;
    config.receptive_field.radius = 2.0f;

    minicolumn_t* col = minicolumn_create(pool, &config);
    ASSERT_NE(col, nullptr);

    // Test at various distances
    struct TestPoint {
        float x, y, z;
        float expected_distance;
    };

    TestPoint points[] = {
        {0.0f, 0.0f, 0.0f, 0.0f},      // At center
        {2.0f, 0.0f, 0.0f, 2.0f},      // 1 sigma away
        {0.0f, 2.0f, 0.0f, 2.0f},      // 1 sigma away (different axis)
        {4.0f, 0.0f, 0.0f, 4.0f},      // 2 sigma away
        {2.0f, 2.0f, 2.0f, std::sqrt(12.0f)}  // Diagonal
    };

    for (const auto& pt : points) {
        float weight = minicolumn_compute_receptive_weight(col, pt.x, pt.y, pt.z);
        float expected = ComputeExpectedGaussianWeight(pt.expected_distance, 2.0f);
        EXPECT_NEAR(weight, expected, TOLERANCE);
    }

    minicolumn_destroy(col);
}

/**
 * WHAT: Test receptive field with invalid radius
 * WHY:  Verify validation of radius parameter
 * HOW:  Try to set zero/negative radius
 */
TEST_F(CorticalColumnTest, ReceptiveField_SetInvalidRadius_NoError) {
    CreateDefaultPool();

    std::vector<uint32_t> neuron_ids = {0, 1, 2};
    minicolumn_config_t config = CreateTestMinicolumnConfig(neuron_ids, 0.0f);
    minicolumn_t* col = minicolumn_create(pool, &config);
    ASSERT_NE(col, nullptr);

    // Try to set invalid radius (should be rejected)
    minicolumn_set_receptive_field(col, 0.0f, 0.0f, 0.0f, 0.0f);
    // Should not crash

    minicolumn_destroy(col);
}

/**
 * WHAT: Test compute_receptive_weight with NULL
 * WHY:  Verify error handling
 * HOW:  Pass NULL, expect error return
 */
TEST_F(CorticalColumnTest, ReceptiveField_ComputeWeightNull_ReturnsError) {
    float weight = minicolumn_compute_receptive_weight(nullptr, 0.0f, 0.0f, 0.0f);
    EXPECT_LT(weight, 0.0f);
}

//=============================================================================
// Statistics Tests
//=============================================================================

/**
 * WHAT: Test minicolumn statistics retrieval
 * WHY:  Verify stats are correctly tracked
 * HOW:  Create, compute, get stats, verify values
 */
TEST_F(CorticalColumnTest, Stats_MinicolumnStats_AccurateValues) {
    CreateDefaultPool();

    std::vector<uint32_t> neuron_ids = {0, 1, 2, 3, 4, 5};
    minicolumn_config_t config = CreateTestMinicolumnConfig(neuron_ids, 90.0f);
    minicolumn_t* col = minicolumn_create(pool, &config);
    ASSERT_NE(col, nullptr);

    // Compute several times
    float input[3] = {0.0f, 0.0f, 0.0f};
    for (int i = 0; i < 10; i++) {
        minicolumn_compute(col, input, 3);
    }

    // Get stats
    minicolumn_stats_t stats;
    minicolumn_get_stats(col, &stats);

    EXPECT_EQ(stats.num_neurons, 6);
    EXPECT_NEAR(stats.tuning_preference, 90.0f, TOLERANCE);
    EXPECT_EQ(stats.total_activations, 10u);
    EXPECT_GE(stats.activation_level, 0.0f);
    EXPECT_LE(stats.activation_level, 1.0f);
    EXPECT_GE(stats.average_activation, 0.0f);
    EXPECT_LE(stats.average_activation, 1.0f);

    minicolumn_destroy(col);
}

/**
 * WHAT: Test minicolumn get_stats with NULL minicolumn
 * WHY:  Verify error handling
 * HOW:  Pass NULL, should not crash
 */
TEST_F(CorticalColumnTest, Stats_MinicolumnGetStatsNull_NoError) {
    minicolumn_stats_t stats;
    minicolumn_get_stats(nullptr, &stats);
    // Should not crash
}

/**
 * WHAT: Test minicolumn get_stats with NULL stats
 * WHY:  Verify error handling
 * HOW:  Pass NULL stats, should not crash
 */
TEST_F(CorticalColumnTest, Stats_MinicolumnGetStatsNullOutput_NoError) {
    CreateDefaultPool();

    std::vector<uint32_t> neuron_ids = {0, 1, 2};
    minicolumn_config_t config = CreateTestMinicolumnConfig(neuron_ids, 0.0f);
    minicolumn_t* col = minicolumn_create(pool, &config);
    ASSERT_NE(col, nullptr);

    minicolumn_get_stats(col, nullptr);
    // Should not crash

    minicolumn_destroy(col);
}

/**
 * WHAT: Test hypercolumn statistics retrieval
 * WHY:  Verify stats are correctly tracked
 * HOW:  Create, compute, get stats, verify values
 */
TEST_F(CorticalColumnTest, Stats_HypercolumnStats_AccurateValues) {
    CreateDefaultPool();

    std::vector<std::vector<uint32_t>> neuron_id_arrays(5);
    std::vector<minicolumn_config_t> mc_configs(5);

    for (int i = 0; i < 5; i++) {
        neuron_id_arrays[i] = {static_cast<uint32_t>(i * 2), static_cast<uint32_t>(i * 2 + 1)};
        mc_configs[i] = CreateTestMinicolumnConfig(neuron_id_arrays[i], i * 36.0f);
    }

    hypercolumn_config_t hc_config = {};
    hc_config.num_minicolumns = 5;
    hc_config.minicolumn_configs = mc_configs.data();
    hc_config.feature_space_min = 0.0f;
    hc_config.feature_space_max = 180.0f;
    hc_config.competition = CC_COMPETITION_WINNER_TAKE_ALL;

    hypercolumn_t* hcol = hypercolumn_create(pool, &hc_config);
    ASSERT_NE(hcol, nullptr);

    // Compute several times
    float input[3] = {0.5f, 0.0f, 0.0f};
    for (int i = 0; i < 5; i++) {
        hypercolumn_compute(hcol, input, 3);
    }

    // Get stats
    cc_hypercolumn_stats_t stats;
    hypercolumn_get_stats(hcol, &stats);

    EXPECT_EQ(stats.num_minicolumns, 5);
    EXPECT_EQ(stats.total_computations, 5u);
    EXPECT_LT(stats.winner_index, 5);
    EXPECT_GE(stats.winner_activation, 0.0f);
    EXPECT_LE(stats.winner_activation, 1.0f);
    EXPECT_GE(stats.total_activation, 0.0f);
    EXPECT_GE(stats.entropy, 0.0f);
    EXPECT_EQ(stats.competition_mode, CC_COMPETITION_WINNER_TAKE_ALL);

    hypercolumn_destroy(hcol);
}

/**
 * WHAT: Test hypercolumn get_stats with NULL hypercolumn
 * WHY:  Verify error handling
 * HOW:  Pass NULL, should not crash
 */
TEST_F(CorticalColumnTest, Stats_HypercolumnGetStatsNull_NoError) {
    cc_hypercolumn_stats_t stats;
    hypercolumn_get_stats(nullptr, &stats);
    // Should not crash
}

/**
 * WHAT: Test hypercolumn get_stats with NULL stats
 * WHY:  Verify error handling
 * HOW:  Pass NULL stats, should not crash
 */
TEST_F(CorticalColumnTest, Stats_HypercolumnGetStatsNullOutput_NoError) {
    CreateDefaultPool();

    std::vector<uint32_t> neuron_ids = {0, 1, 2};
    minicolumn_config_t mc_config = CreateTestMinicolumnConfig(neuron_ids, 0.0f);

    hypercolumn_config_t hc_config = {};
    hc_config.num_minicolumns = 1;
    hc_config.minicolumn_configs = &mc_config;
    hc_config.feature_space_min = 0.0f;
    hc_config.feature_space_max = 180.0f;
    hc_config.competition = CC_COMPETITION_WINNER_TAKE_ALL;

    hypercolumn_t* hcol = hypercolumn_create(pool, &hc_config);
    ASSERT_NE(hcol, nullptr);

    hypercolumn_get_stats(hcol, nullptr);
    // Should not crash

    hypercolumn_destroy(hcol);
}

/**
 * WHAT: Test entropy calculation
 * WHY:  Verify entropy is computed correctly
 * HOW:  Create uniform and peaked distributions, verify entropy values
 */
TEST_F(CorticalColumnTest, Stats_Entropy_CorrectCalculation) {
    CreateDefaultPool();

    std::vector<std::vector<uint32_t>> neuron_id_arrays(5);
    std::vector<minicolumn_config_t> mc_configs(5);

    for (int i = 0; i < 5; i++) {
        neuron_id_arrays[i] = {static_cast<uint32_t>(i)};
        mc_configs[i] = CreateTestMinicolumnConfig(neuron_id_arrays[i], i * 36.0f);
        // All same receptive field for uniform activation
        mc_configs[i].receptive_field.center_x = 0.5f;
        mc_configs[i].receptive_field.radius = 1.0f;
    }

    // Test with SOFTMAX (should have medium entropy)
    hypercolumn_config_t hc_config = {};
    hc_config.num_minicolumns = 5;
    hc_config.minicolumn_configs = mc_configs.data();
    hc_config.feature_space_min = 0.0f;
    hc_config.feature_space_max = 180.0f;
    hc_config.competition = CC_COMPETITION_SOFTMAX;
    hc_config.temperature = 1.0f;

    hypercolumn_t* hcol_softmax = hypercolumn_create(pool, &hc_config);
    ASSERT_NE(hcol_softmax, nullptr);

    float input[3] = {0.5f, 0.0f, 0.0f};
    hypercolumn_compute(hcol_softmax, input, 3);

    cc_hypercolumn_stats_t stats_softmax;
    hypercolumn_get_stats(hcol_softmax, &stats_softmax);
    float entropy_softmax = stats_softmax.entropy;

    // Test with WTA (should have low entropy)
    hc_config.competition = CC_COMPETITION_WINNER_TAKE_ALL;
    hypercolumn_t* hcol_wta = hypercolumn_create(pool, &hc_config);
    ASSERT_NE(hcol_wta, nullptr);

    hypercolumn_compute(hcol_wta, input, 3);

    cc_hypercolumn_stats_t stats_wta;
    hypercolumn_get_stats(hcol_wta, &stats_wta);
    float entropy_wta = stats_wta.entropy;

    // WTA should have lower entropy than softmax
    EXPECT_LT(entropy_wta, entropy_softmax);

    hypercolumn_destroy(hcol_softmax);
    hypercolumn_destroy(hcol_wta);
}

//=============================================================================
// Integration Tests
//=============================================================================

/**
 * WHAT: Test complete workflow with realistic data
 * WHY:  Verify end-to-end functionality
 * HOW:  Create hypercolumn, run multiple compute cycles, verify consistency
 */
TEST_F(CorticalColumnTest, Integration_CompleteWorkflow_Success) {
    CreateDefaultPool();

    // Create orientation-selective hypercolumn (simulating V1)
    const int num_orientations = 12;
    std::vector<std::vector<uint32_t>> neuron_id_arrays(num_orientations);
    std::vector<minicolumn_config_t> mc_configs(num_orientations);

    for (int i = 0; i < num_orientations; i++) {
        // 80 neurons per minicolumn
        neuron_id_arrays[i].resize(80);
        std::iota(neuron_id_arrays[i].begin(), neuron_id_arrays[i].end(), i * 80);

        float orientation = i * 15.0f;  // 0°, 15°, 30°, ..., 165°
        mc_configs[i] = CreateTestMinicolumnConfig(neuron_id_arrays[i], orientation);
        mc_configs[i].receptive_field.center_x = std::cos(orientation * M_PI / 180.0f);
        mc_configs[i].receptive_field.center_y = std::sin(orientation * M_PI / 180.0f);
        mc_configs[i].receptive_field.center_z = 0.0f;
        mc_configs[i].receptive_field.radius = 0.5f;
    }

    hypercolumn_config_t hc_config = {};
    hc_config.num_minicolumns = num_orientations;
    hc_config.minicolumn_configs = mc_configs.data();
    hc_config.feature_space_min = 0.0f;
    hc_config.feature_space_max = 180.0f;
    hc_config.competition = CC_COMPETITION_K_WINNERS;
    hc_config.k_winners = 3;
    hc_config.temperature = 1.0f;

    hypercolumn_t* hcol = hypercolumn_create(pool, &hc_config);
    ASSERT_NE(hcol, nullptr);

    // Test with various orientations
    for (int trial = 0; trial < 10; trial++) {
        float angle = trial * 18.0f;  // 0°, 18°, 36°, ...
        float input[3] = {
            std::cos(angle * M_PI / 180.0f),
            std::sin(angle * M_PI / 180.0f),
            0.0f
        };

        hypercolumn_compute(hcol, input, 3);

        // Verify winner is reasonable
        uint32_t winner = hypercolumn_get_winner(hcol);
        EXPECT_LT(winner, static_cast<uint32_t>(num_orientations));

        // Get distribution and verify K winners
        float distribution[12] = {0.0f};
        hypercolumn_get_distribution(hcol, distribution, num_orientations);

        int active_count = 0;
        for (int i = 0; i < num_orientations; i++) {
            if (distribution[i] > 0.5f) {
                active_count++;
            }
        }
        EXPECT_EQ(active_count, 3);  // K=3

        // Verify stats
        cc_hypercolumn_stats_t stats;
        hypercolumn_get_stats(hcol, &stats);
        EXPECT_EQ(stats.total_computations, static_cast<uint32_t>(trial + 1));
    }

    hypercolumn_destroy(hcol);
}

/**
 * WHAT: Test memory reuse after destroy
 * WHY:  Verify pool properly recycles memory
 * HOW:  Create/destroy multiple times, verify no leaks
 */
TEST_F(CorticalColumnTest, Integration_MemoryReuse_NoLeaks) {
    CreateDefaultPool();

    std::vector<uint32_t> neuron_ids = {0, 1, 2, 3, 4, 5};
    minicolumn_config_t config = CreateTestMinicolumnConfig(neuron_ids, 45.0f);

    // Create and destroy multiple times
    for (int i = 0; i < 20; i++) {
        minicolumn_t* col = minicolumn_create(pool, &config);
        ASSERT_NE(col, nullptr);

        float input[3] = {0.0f, 0.0f, 0.0f};
        float activation = minicolumn_compute(col, input, 3);
        EXPECT_GE(activation, 0.0f);

        minicolumn_destroy(col);
    }
    // Should complete without exhausting pool
}

//=============================================================================
// Main Entry Point
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
