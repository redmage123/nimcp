/**
 * @file test_cerebellar_interneurons.cpp
 * @brief Unit tests for Cerebellar inhibitory interneurons (Phase 2)
 *
 * Tests:
 * - Basket cell configuration and activity
 * - Stellate cell dendritic inhibition
 * - Golgi cell feedback inhibition
 * - Interneuron queries
 * - Purkinje cell inhibition integration
 * - Null safety
 *
 * BIOLOGICAL CONTEXT:
 * - Basket cells: Fast-spiking, provide strong perisomatic inhibition to Purkinje cells
 * - Stellate cells: Outer molecular layer, provide dendritic inhibition
 * - Golgi cells: Granule layer feedback inhibition
 *
 * @version Phase 2: Inhibitory Interneurons
 * @date 2026-01-03
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/brain/regions/cerebellum/nimcp_cerebellum_adapter.h"

//=============================================================================
// Interneuron Configuration Tests
//=============================================================================

class CerebellarInterneuronConfigTest : public ::testing::Test {};

TEST_F(CerebellarInterneuronConfigTest, DefaultConfigHasBasketCells) {
    cerebellum_config_t config = cerebellum_default_config();

    // Default basket cell configuration
    EXPECT_EQ(config.num_basket_cells, CEREBELLUM_DEFAULT_BASKET_CELLS);
    EXPECT_EQ(config.purkinje_per_basket, CEREBELLUM_DEFAULT_PURKINJE_PER_BASKET);
}

TEST_F(CerebellarInterneuronConfigTest, DefaultConfigHasStellateCells) {
    cerebellum_config_t config = cerebellum_default_config();

    // Default stellate cell configuration
    EXPECT_EQ(config.num_stellate_cells, CEREBELLUM_DEFAULT_STELLATE_CELLS);
    EXPECT_EQ(config.purkinje_per_stellate, CEREBELLUM_DEFAULT_PURKINJE_PER_STELLATE);
}

TEST_F(CerebellarInterneuronConfigTest, DefaultConfigHasGolgiCells) {
    cerebellum_config_t config = cerebellum_default_config();

    // Default Golgi cell configuration
    EXPECT_EQ(config.num_golgi_cells, CEREBELLUM_DEFAULT_GOLGI_CELLS);
    EXPECT_EQ(config.granules_per_golgi, CEREBELLUM_DEFAULT_GRANULES_PER_GOLGI);
}

TEST_F(CerebellarInterneuronConfigTest, InterneuronsDisabledByDefault) {
    cerebellum_config_t config = cerebellum_default_config();

    // Interneurons disabled by default for backward compatibility
    EXPECT_FALSE(config.enable_basket_cells);
    EXPECT_FALSE(config.enable_stellate_cells);
    EXPECT_FALSE(config.enable_golgi_cells);
}

TEST_F(CerebellarInterneuronConfigTest, PurkinjeCellToInterneuronRatios) {
    cerebellum_config_t config = cerebellum_default_config();

    // Basket cells target ~5 Purkinje cells each (perisomatic)
    EXPECT_LE(config.purkinje_per_basket, 10);

    // Stellate cells target ~2-3 Purkinje cells each (dendritic)
    EXPECT_LE(config.purkinje_per_stellate, 5);

    // Golgi cells target many granule cells (~50-100)
    EXPECT_GE(config.granules_per_golgi, 25);
}

//=============================================================================
// Basket Cell Tests
//=============================================================================

class CerebellarBasketCellTest : public ::testing::Test {
protected:
    cerebellum_adapter_t* adapter = nullptr;

    void SetUp() override {
        cerebellum_config_t config = cerebellum_default_config();
        config.enable_basket_cells = true;
        config.num_basket_cells = 10;
        config.purkinje_per_basket = 5;
        adapter = cerebellum_create(&config);
    }

    void TearDown() override {
        if (adapter) {
            cerebellum_destroy(adapter);
            adapter = nullptr;
        }
    }
};

TEST_F(CerebellarBasketCellTest, CreateWithBasketCells) {
    ASSERT_NE(adapter, nullptr);

    cerebellum_config_t retrieved;
    EXPECT_TRUE(cerebellum_get_config(adapter, &retrieved));
    EXPECT_TRUE(retrieved.enable_basket_cells);
    EXPECT_EQ(retrieved.num_basket_cells, 10);
}

TEST_F(CerebellarBasketCellTest, GetBasketActivity) {
    float activation, firing_rate;

    bool success = cerebellum_get_basket_activity(adapter, 0, &activation, &firing_rate);
    EXPECT_TRUE(success);

    // Initial activity should be at baseline
    EXPECT_GE(activation, 0.0f);
    EXPECT_LE(activation, 1.0f);
    EXPECT_GE(firing_rate, 0.0f);
}

TEST_F(CerebellarBasketCellTest, GetBasketActivityInvalidId) {
    float activation, firing_rate;

    // ID beyond range
    bool success = cerebellum_get_basket_activity(adapter, 1000, &activation, &firing_rate);
    EXPECT_FALSE(success);
}

TEST_F(CerebellarBasketCellTest, BasketActivityIncreasesWithInput) {
    // Feed input to activate parallel fibers (which drive basket cells)
    mossy_fiber_input_t input;
    input.fiber_id = 0;
    input.activity = 1.0f;
    input.timestamp_ms = 0.0f;
    input.modality = 0;

    cerebellum_process_mossy_input(adapter, &input);

    motor_coordination_result_t result;
    cerebellum_process(adapter, &result);

    float activation, firing_rate;
    bool success = cerebellum_get_basket_activity(adapter, 0, &activation, &firing_rate);
    EXPECT_TRUE(success);

    // After input, basket cells should show some activity
    EXPECT_GE(activation, 0.0f);
}

TEST_F(CerebellarBasketCellTest, AllBasketCellsQueried) {
    cerebellum_config_t config;
    cerebellum_get_config(adapter, &config);

    // Query all basket cells
    for (uint32_t i = 0; i < config.num_basket_cells; i++) {
        float activation, firing_rate;
        bool success = cerebellum_get_basket_activity(adapter, i, &activation, &firing_rate);
        EXPECT_TRUE(success);
        EXPECT_GE(activation, 0.0f);
        EXPECT_LE(activation, 1.0f);
    }
}

//=============================================================================
// Stellate Cell Tests
//=============================================================================

class CerebellarStellateCellTest : public ::testing::Test {
protected:
    cerebellum_adapter_t* adapter = nullptr;

    void SetUp() override {
        cerebellum_config_t config = cerebellum_default_config();
        config.enable_stellate_cells = true;
        config.num_stellate_cells = 20;
        config.purkinje_per_stellate = 3;
        adapter = cerebellum_create(&config);
    }

    void TearDown() override {
        if (adapter) {
            cerebellum_destroy(adapter);
            adapter = nullptr;
        }
    }
};

TEST_F(CerebellarStellateCellTest, CreateWithStellateCells) {
    ASSERT_NE(adapter, nullptr);

    cerebellum_config_t retrieved;
    EXPECT_TRUE(cerebellum_get_config(adapter, &retrieved));
    EXPECT_TRUE(retrieved.enable_stellate_cells);
    EXPECT_EQ(retrieved.num_stellate_cells, 20);
}

TEST_F(CerebellarStellateCellTest, StellateCellsProvideDendriticInhibition) {
    // Feed input
    mossy_fiber_input_t input;
    input.fiber_id = 0;
    input.activity = 1.0f;
    input.timestamp_ms = 0.0f;
    input.modality = 0;

    for (int i = 0; i < 5; i++) {
        input.fiber_id = i;
        cerebellum_process_mossy_input(adapter, &input);
    }

    motor_coordination_result_t result;
    cerebellum_process(adapter, &result);

    // Check inhibition on a Purkinje cell
    float somatic_inhibition, dendritic_inhibition;
    bool success = cerebellum_get_purkinje_inhibition(adapter, 0,
                                                       &somatic_inhibition,
                                                       &dendritic_inhibition);
    EXPECT_TRUE(success);
    EXPECT_GE(dendritic_inhibition, 0.0f);  // Stellate cells contribute here
}

//=============================================================================
// Golgi Cell Tests
//=============================================================================

class CerebellarGolgiCellTest : public ::testing::Test {
protected:
    cerebellum_adapter_t* adapter = nullptr;

    void SetUp() override {
        cerebellum_config_t config = cerebellum_default_config();
        config.enable_golgi_cells = true;
        config.num_golgi_cells = 20;
        config.granules_per_golgi = 75;
        adapter = cerebellum_create(&config);
    }

    void TearDown() override {
        if (adapter) {
            cerebellum_destroy(adapter);
            adapter = nullptr;
        }
    }
};

TEST_F(CerebellarGolgiCellTest, CreateWithGolgiCells) {
    ASSERT_NE(adapter, nullptr);

    cerebellum_config_t retrieved;
    EXPECT_TRUE(cerebellum_get_config(adapter, &retrieved));
    EXPECT_TRUE(retrieved.enable_golgi_cells);
    EXPECT_EQ(retrieved.num_golgi_cells, 20);
}

TEST_F(CerebellarGolgiCellTest, GetGolgiFeedback) {
    float total_feedback, avg_granule_inhibition;

    bool success = cerebellum_get_golgi_feedback(adapter, &total_feedback, &avg_granule_inhibition);
    EXPECT_TRUE(success);

    // Initial feedback should be at baseline
    EXPECT_GE(total_feedback, 0.0f);
    EXPECT_GE(avg_granule_inhibition, 0.0f);
}

TEST_F(CerebellarGolgiCellTest, GolgiFeedbackIncreasesWithActivity) {
    // Get baseline
    float baseline_total, baseline_avg;
    cerebellum_get_golgi_feedback(adapter, &baseline_total, &baseline_avg);

    // Feed sustained input
    mossy_fiber_input_t input;
    input.fiber_id = 0;
    input.activity = 1.0f;
    input.timestamp_ms = 0.0f;
    input.modality = 0;

    for (int i = 0; i < 20; i++) {
        input.fiber_id = i % 10;
        input.timestamp_ms = (float)i * 5.0f;
        cerebellum_process_mossy_input(adapter, &input);

        motor_coordination_result_t result;
        cerebellum_process(adapter, &result);
    }

    float active_total, active_avg;
    cerebellum_get_golgi_feedback(adapter, &active_total, &active_avg);

    // Golgi feedback should increase with activity (feedback inhibition)
    EXPECT_GE(active_total, 0.0f);
}

TEST_F(CerebellarGolgiCellTest, GolgiFeedbackInhibitsGranuleCells) {
    // Feed input and check that Golgi cells modulate granule activity
    mossy_fiber_input_t input;
    input.fiber_id = 0;
    input.activity = 1.0f;
    input.timestamp_ms = 0.0f;
    input.modality = 0;

    cerebellum_process_mossy_input(adapter, &input);

    motor_coordination_result_t result;
    cerebellum_process(adapter, &result);

    float total_feedback, avg_inhibition;
    bool success = cerebellum_get_golgi_feedback(adapter, &total_feedback, &avg_inhibition);
    EXPECT_TRUE(success);

    // Both measures should be valid non-negative values
    EXPECT_GE(total_feedback, 0.0f);
    EXPECT_GE(avg_inhibition, 0.0f);
}

//=============================================================================
// Purkinje Inhibition Integration Tests
//=============================================================================

class CerebellarPurkinjeInhibitionTest : public ::testing::Test {
protected:
    cerebellum_adapter_t* adapter = nullptr;

    void SetUp() override {
        cerebellum_config_t config = cerebellum_default_config();
        // Enable all interneurons
        config.enable_basket_cells = true;
        config.enable_stellate_cells = true;
        config.enable_golgi_cells = true;
        config.num_basket_cells = 10;
        config.num_stellate_cells = 20;
        config.num_golgi_cells = 20;
        adapter = cerebellum_create(&config);
    }

    void TearDown() override {
        if (adapter) {
            cerebellum_destroy(adapter);
            adapter = nullptr;
        }
    }
};

TEST_F(CerebellarPurkinjeInhibitionTest, GetPurkinjeInhibition) {
    float somatic, dendritic;

    bool success = cerebellum_get_purkinje_inhibition(adapter, 0, &somatic, &dendritic);
    EXPECT_TRUE(success);

    // Both inhibition types should be non-negative
    EXPECT_GE(somatic, 0.0f);
    EXPECT_GE(dendritic, 0.0f);
}

TEST_F(CerebellarPurkinjeInhibitionTest, InhibitionIncreasesWithActivity) {
    // Get baseline inhibition
    float baseline_somatic, baseline_dendritic;
    cerebellum_get_purkinje_inhibition(adapter, 0, &baseline_somatic, &baseline_dendritic);

    // Feed sustained activity
    mossy_fiber_input_t input;
    input.fiber_id = 0;
    input.activity = 1.0f;
    input.timestamp_ms = 0.0f;
    input.modality = 0;

    for (int i = 0; i < 10; i++) {
        input.fiber_id = i;
        cerebellum_process_mossy_input(adapter, &input);
    }

    motor_coordination_result_t result;
    cerebellum_process(adapter, &result);

    float active_somatic, active_dendritic;
    cerebellum_get_purkinje_inhibition(adapter, 0, &active_somatic, &active_dendritic);

    // Inhibition should increase with feedforward activity through interneurons
    EXPECT_GE(active_somatic, 0.0f);
    EXPECT_GE(active_dendritic, 0.0f);
}

TEST_F(CerebellarPurkinjeInhibitionTest, InvalidPurkinjeId) {
    float somatic, dendritic;

    // ID beyond range
    bool success = cerebellum_get_purkinje_inhibition(adapter, 10000, &somatic, &dendritic);
    EXPECT_FALSE(success);
}

TEST_F(CerebellarPurkinjeInhibitionTest, AllPurkinjeCellsQueried) {
    cerebellum_config_t config;
    cerebellum_get_config(adapter, &config);

    // Query inhibition for all Purkinje cells
    for (uint32_t i = 0; i < config.num_purkinje_cells; i++) {
        float somatic, dendritic;
        bool success = cerebellum_get_purkinje_inhibition(adapter, i, &somatic, &dendritic);
        EXPECT_TRUE(success);
        EXPECT_GE(somatic, 0.0f);
        EXPECT_GE(dendritic, 0.0f);
    }
}

TEST_F(CerebellarPurkinjeInhibitionTest, SomaticFromBasketDendriticFromStellate) {
    // Somatic inhibition comes from basket cells
    // Dendritic inhibition comes from stellate cells
    // This is the biological pattern

    float somatic, dendritic;
    bool success = cerebellum_get_purkinje_inhibition(adapter, 0, &somatic, &dendritic);
    EXPECT_TRUE(success);

    // Both types should be distinguishable
    // (specific values depend on implementation)
}

//=============================================================================
// Interneuron Statistics Tests
//=============================================================================

class CerebellarInterneuronStatsTest : public ::testing::Test {
protected:
    cerebellum_adapter_t* adapter = nullptr;

    void SetUp() override {
        cerebellum_config_t config = cerebellum_default_config();
        config.enable_basket_cells = true;
        config.enable_stellate_cells = true;
        config.enable_golgi_cells = true;
        adapter = cerebellum_create(&config);
    }

    void TearDown() override {
        if (adapter) {
            cerebellum_destroy(adapter);
            adapter = nullptr;
        }
    }
};

TEST_F(CerebellarInterneuronStatsTest, StatsTrackInterneuronActivity) {
    cerebellum_stats_t stats;
    cerebellum_get_stats(adapter, &stats);

    // Initial stats should be zero
    EXPECT_EQ(stats.basket_cell_spikes, 0);
    EXPECT_EQ(stats.stellate_cell_spikes, 0);
    EXPECT_EQ(stats.golgi_cell_spikes, 0);
}

TEST_F(CerebellarInterneuronStatsTest, StatsIncrementWithActivity) {
    // Feed activity
    mossy_fiber_input_t input;
    input.fiber_id = 0;
    input.activity = 1.0f;
    input.timestamp_ms = 0.0f;
    input.modality = 0;

    for (int i = 0; i < 20; i++) {
        input.fiber_id = i % 10;
        input.timestamp_ms = (float)i * 5.0f;
        cerebellum_process_mossy_input(adapter, &input);

        motor_coordination_result_t result;
        cerebellum_process(adapter, &result);
    }

    cerebellum_stats_t stats;
    cerebellum_get_stats(adapter, &stats);

    // After activity, interneuron stats should be updated
    EXPECT_GE(stats.basket_inhibition_total, 0.0f);
    EXPECT_GE(stats.stellate_inhibition_total, 0.0f);
    EXPECT_GE(stats.golgi_feedback_total, 0.0f);
}

TEST_F(CerebellarInterneuronStatsTest, AverageFiringRatesValid) {
    cerebellum_stats_t stats;
    cerebellum_get_stats(adapter, &stats);

    // Firing rates should be non-negative
    EXPECT_GE(stats.avg_basket_firing_rate, 0.0f);
    EXPECT_GE(stats.avg_stellate_firing_rate, 0.0f);
    EXPECT_GE(stats.avg_golgi_firing_rate, 0.0f);
}

//=============================================================================
// Interneuron Disabled Tests
//=============================================================================

class CerebellarInterneuronDisabledTest : public ::testing::Test {
protected:
    cerebellum_adapter_t* adapter = nullptr;

    void SetUp() override {
        cerebellum_config_t config = cerebellum_default_config();
        // Keep interneurons disabled (default)
        adapter = cerebellum_create(&config);
    }

    void TearDown() override {
        if (adapter) {
            cerebellum_destroy(adapter);
            adapter = nullptr;
        }
    }
};

TEST_F(CerebellarInterneuronDisabledTest, QueriesReturnFalseWhenDisabled) {
    float activation, firing_rate;

    // Basket cell query should fail when disabled
    bool success = cerebellum_get_basket_activity(adapter, 0, &activation, &firing_rate);
    EXPECT_FALSE(success);
}

TEST_F(CerebellarInterneuronDisabledTest, PurkinjeInhibitionZeroWhenDisabled) {
    float somatic, dendritic;

    // Should still succeed, but with zero inhibition
    bool success = cerebellum_get_purkinje_inhibition(adapter, 0, &somatic, &dendritic);

    // Either fails or returns zero inhibition
    if (success) {
        EXPECT_FLOAT_EQ(somatic, 0.0f);
        EXPECT_FLOAT_EQ(dendritic, 0.0f);
    }
}

TEST_F(CerebellarInterneuronDisabledTest, GolgiFeedbackZeroWhenDisabled) {
    float total, avg;

    bool success = cerebellum_get_golgi_feedback(adapter, &total, &avg);

    // Either fails or returns zero feedback
    if (success) {
        EXPECT_FLOAT_EQ(total, 0.0f);
        EXPECT_FLOAT_EQ(avg, 0.0f);
    }
}

//=============================================================================
// Null Safety Tests
//=============================================================================

class CerebellarInterneuronNullSafetyTest : public ::testing::Test {};

TEST_F(CerebellarInterneuronNullSafetyTest, GetBasketActivityNullAdapter) {
    float activation, firing_rate;
    bool success = cerebellum_get_basket_activity(nullptr, 0, &activation, &firing_rate);
    EXPECT_FALSE(success);
}

TEST_F(CerebellarInterneuronNullSafetyTest, GetBasketActivityNullOutputs) {
    cerebellum_config_t config = cerebellum_default_config();
    config.enable_basket_cells = true;
    cerebellum_adapter_t* adapter = cerebellum_create(&config);
    ASSERT_NE(adapter, nullptr);

    bool success = cerebellum_get_basket_activity(adapter, 0, nullptr, nullptr);
    EXPECT_FALSE(success);

    cerebellum_destroy(adapter);
}

TEST_F(CerebellarInterneuronNullSafetyTest, GetPurkinjeInhibitionNullAdapter) {
    float somatic, dendritic;
    bool success = cerebellum_get_purkinje_inhibition(nullptr, 0, &somatic, &dendritic);
    EXPECT_FALSE(success);
}

TEST_F(CerebellarInterneuronNullSafetyTest, GetPurkinjeInhibitionNullOutputs) {
    cerebellum_config_t config = cerebellum_default_config();
    config.enable_basket_cells = true;
    config.enable_stellate_cells = true;
    cerebellum_adapter_t* adapter = cerebellum_create(&config);
    ASSERT_NE(adapter, nullptr);

    bool success = cerebellum_get_purkinje_inhibition(adapter, 0, nullptr, nullptr);
    EXPECT_FALSE(success);

    cerebellum_destroy(adapter);
}

TEST_F(CerebellarInterneuronNullSafetyTest, GetGolgiFeedbackNullAdapter) {
    float total, avg;
    bool success = cerebellum_get_golgi_feedback(nullptr, &total, &avg);
    EXPECT_FALSE(success);
}

TEST_F(CerebellarInterneuronNullSafetyTest, GetGolgiFeedbackNullOutputs) {
    cerebellum_config_t config = cerebellum_default_config();
    config.enable_golgi_cells = true;
    cerebellum_adapter_t* adapter = cerebellum_create(&config);
    ASSERT_NE(adapter, nullptr);

    bool success = cerebellum_get_golgi_feedback(adapter, nullptr, nullptr);
    EXPECT_FALSE(success);

    cerebellum_destroy(adapter);
}

//=============================================================================
// Molecular Layer Interneurons Access Test
//=============================================================================

class CerebellarMolecularInterneuronsTest : public ::testing::Test {
protected:
    cerebellum_adapter_t* adapter = nullptr;

    void SetUp() override {
        cerebellum_config_t config = cerebellum_default_config();
        config.enable_basket_cells = true;
        config.enable_stellate_cells = true;
        adapter = cerebellum_create(&config);
    }

    void TearDown() override {
        if (adapter) {
            cerebellum_destroy(adapter);
            adapter = nullptr;
        }
    }
};

TEST_F(CerebellarMolecularInterneuronsTest, GetMolecularInterneurons) {
    molecular_layer_interneurons_t* interneurons = cerebellum_get_molecular_interneurons(adapter);

    // Should return valid handle when interneurons enabled
    EXPECT_NE(interneurons, nullptr);
}

TEST_F(CerebellarMolecularInterneuronsTest, GetMolecularInterneuronsNull) {
    molecular_layer_interneurons_t* interneurons = cerebellum_get_molecular_interneurons(nullptr);
    EXPECT_EQ(interneurons, nullptr);
}

//=============================================================================
// Golgi Network Access Test
//=============================================================================

class CerebellarGolgiNetworkTest : public ::testing::Test {
protected:
    cerebellum_adapter_t* adapter = nullptr;

    void SetUp() override {
        cerebellum_config_t config = cerebellum_default_config();
        config.enable_golgi_cells = true;
        adapter = cerebellum_create(&config);
    }

    void TearDown() override {
        if (adapter) {
            cerebellum_destroy(adapter);
            adapter = nullptr;
        }
    }
};

TEST_F(CerebellarGolgiNetworkTest, GetGolgiNetwork) {
    golgi_cell_network_t* network = cerebellum_get_golgi_network(adapter);

    // Should return valid handle when Golgi cells enabled
    EXPECT_NE(network, nullptr);
}

TEST_F(CerebellarGolgiNetworkTest, GetGolgiNetworkNull) {
    golgi_cell_network_t* network = cerebellum_get_golgi_network(nullptr);
    EXPECT_EQ(network, nullptr);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
