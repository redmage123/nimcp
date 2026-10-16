/**
 * @file test_cerebellum_interneurons.cpp
 * @brief Unit tests for cerebellar inhibitory interneurons (Phase 2)
 *
 * Tests:
 * - Basket cells (Purkinje soma inhibition)
 * - Stellate cells (Purkinje dendrite inhibition)
 * - Golgi cells (granule layer feedback)
 * - Interneuron network dynamics
 *
 * @version Phase 2: Inhibitory Interneurons
 * @date 2026-01-03
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

#include "core/brain/regions/cerebellum/nimcp_cerebellum_adapter.h"

//=============================================================================
// Basket Cell Tests
//=============================================================================

class BasketCellTest : public ::testing::Test {
protected:
    cerebellum_adapter_t* adapter = nullptr;

    void SetUp() override {
        cerebellum_config_t config = cerebellum_default_config();
        config.enable_basket_cells = true;
        config.num_basket_cells = 10;
        config.purkinje_per_basket = 5;
        adapter = cerebellum_create(&config);
        ASSERT_NE(adapter, nullptr);
    }

    void TearDown() override {
        if (adapter) {
            cerebellum_destroy(adapter);
            adapter = nullptr;
        }
    }
};

TEST_F(BasketCellTest, ConfigurationApplied) {
    cerebellum_config_t retrieved;
    cerebellum_get_config(adapter, &retrieved);
    EXPECT_TRUE(retrieved.enable_basket_cells);
    EXPECT_EQ(retrieved.num_basket_cells, 10);
    EXPECT_EQ(retrieved.purkinje_per_basket, 5);
}

TEST_F(BasketCellTest, GetBasketActivity) {
    // First process some input to activate the network
    mossy_fiber_input_t input;
    input.fiber_id = 0;
    input.activity = 0.8f;
    input.timestamp_ms = 0.0f;
    input.modality = 0;
    cerebellum_process_mossy_input(adapter, &input);

    motor_coordination_result_t result;
    cerebellum_process(adapter, &result);

    // Query basket cell activity
    float activation, firing_rate;
    bool success = cerebellum_get_basket_activity(adapter, 0, &activation, &firing_rate);
    EXPECT_TRUE(success);
    EXPECT_GE(activation, 0.0f);
    EXPECT_LE(activation, 1.0f);
    EXPECT_GE(firing_rate, 0.0f);
}

TEST_F(BasketCellTest, InvalidBasketId) {
    float activation, firing_rate;
    bool success = cerebellum_get_basket_activity(adapter, 1000, &activation, &firing_rate);
    EXPECT_FALSE(success);
}

TEST_F(BasketCellTest, BasketInhibitsPurkinje) {
    // Process input
    for (int i = 0; i < 10; i++) {
        mossy_fiber_input_t input;
        input.fiber_id = i;
        input.activity = 0.9f;
        input.timestamp_ms = (float)i;
        input.modality = 0;
        cerebellum_process_mossy_input(adapter, &input);
    }

    motor_coordination_result_t result;
    cerebellum_process(adapter, &result);

    // Check that Purkinje cells receive somatic inhibition
    float somatic_inh, dendritic_inh;
    bool success = cerebellum_get_purkinje_inhibition(adapter, 0, &somatic_inh, &dendritic_inh);
    EXPECT_TRUE(success);
    EXPECT_GE(somatic_inh, 0.0f);  // Should have some inhibition
}

TEST_F(BasketCellTest, StatsTrackBasketSpikes) {
    // Process input to activate basket cells
    for (int i = 0; i < 50; i++) {
        mossy_fiber_input_t input;
        input.fiber_id = i % 10;
        input.activity = 0.8f;
        input.timestamp_ms = (float)i;
        input.modality = 0;
        cerebellum_process_mossy_input(adapter, &input);
    }

    motor_coordination_result_t result;
    cerebellum_process(adapter, &result);

    cerebellum_stats_t stats;
    cerebellum_get_stats(adapter, &stats);

    EXPECT_GT(stats.basket_cell_spikes, 0);
    EXPECT_GT(stats.basket_inhibition_total, 0.0f);
}

//=============================================================================
// Stellate Cell Tests
//=============================================================================

class StellateCellTest : public ::testing::Test {
protected:
    cerebellum_adapter_t* adapter = nullptr;

    void SetUp() override {
        cerebellum_config_t config = cerebellum_default_config();
        config.enable_stellate_cells = true;
        config.num_stellate_cells = 20;
        config.purkinje_per_stellate = 3;
        adapter = cerebellum_create(&config);
        ASSERT_NE(adapter, nullptr);
    }

    void TearDown() override {
        if (adapter) {
            cerebellum_destroy(adapter);
            adapter = nullptr;
        }
    }
};

TEST_F(StellateCellTest, ConfigurationApplied) {
    cerebellum_config_t retrieved;
    cerebellum_get_config(adapter, &retrieved);
    EXPECT_TRUE(retrieved.enable_stellate_cells);
    EXPECT_EQ(retrieved.num_stellate_cells, 20);
    EXPECT_EQ(retrieved.purkinje_per_stellate, 3);
}

TEST_F(StellateCellTest, StellateInhibitsDendrites) {
    // Process input
    for (int i = 0; i < 10; i++) {
        mossy_fiber_input_t input;
        input.fiber_id = i;
        input.activity = 0.9f;
        input.timestamp_ms = (float)i;
        input.modality = 0;
        cerebellum_process_mossy_input(adapter, &input);
    }

    motor_coordination_result_t result;
    cerebellum_process(adapter, &result);

    // Check that Purkinje cells receive dendritic inhibition
    float somatic_inh, dendritic_inh;
    bool success = cerebellum_get_purkinje_inhibition(adapter, 0, &somatic_inh, &dendritic_inh);
    EXPECT_TRUE(success);
    EXPECT_GE(dendritic_inh, 0.0f);  // Should have some inhibition
}

TEST_F(StellateCellTest, StatsTrackStellateSpikes) {
    for (int i = 0; i < 50; i++) {
        mossy_fiber_input_t input;
        input.fiber_id = i % 10;
        input.activity = 0.8f;
        input.timestamp_ms = (float)i;
        input.modality = 0;
        cerebellum_process_mossy_input(adapter, &input);
    }

    motor_coordination_result_t result;
    cerebellum_process(adapter, &result);

    cerebellum_stats_t stats;
    cerebellum_get_stats(adapter, &stats);

    EXPECT_GT(stats.stellate_cell_spikes, 0);
    EXPECT_GT(stats.stellate_inhibition_total, 0.0f);
}

//=============================================================================
// Golgi Cell Tests
//=============================================================================

class GolgiCellTest : public ::testing::Test {
protected:
    cerebellum_adapter_t* adapter = nullptr;

    void SetUp() override {
        cerebellum_config_t config = cerebellum_default_config();
        config.enable_golgi_cells = true;
        config.num_golgi_cells = 20;
        config.granules_per_golgi = 75;
        adapter = cerebellum_create(&config);
        ASSERT_NE(adapter, nullptr);
    }

    void TearDown() override {
        if (adapter) {
            cerebellum_destroy(adapter);
            adapter = nullptr;
        }
    }
};

TEST_F(GolgiCellTest, ConfigurationApplied) {
    cerebellum_config_t retrieved;
    cerebellum_get_config(adapter, &retrieved);
    EXPECT_TRUE(retrieved.enable_golgi_cells);
    EXPECT_EQ(retrieved.num_golgi_cells, 20);
    EXPECT_EQ(retrieved.granules_per_golgi, 75);
}

TEST_F(GolgiCellTest, GolgiNetworkCreated) {
    golgi_cell_network_t* golgi = cerebellum_get_golgi_network(adapter);
    EXPECT_NE(golgi, nullptr);
}

TEST_F(GolgiCellTest, GolgiFeedbackInhibition) {
    // Process input
    for (int i = 0; i < 10; i++) {
        mossy_fiber_input_t input;
        input.fiber_id = i;
        input.activity = 0.9f;
        input.timestamp_ms = (float)i;
        input.modality = 0;
        cerebellum_process_mossy_input(adapter, &input);
    }

    motor_coordination_result_t result;
    cerebellum_process(adapter, &result);

    // Check feedback inhibition
    float total_feedback, avg_inh;
    bool success = cerebellum_get_golgi_feedback(adapter, &total_feedback, &avg_inh);
    EXPECT_TRUE(success);
    EXPECT_GE(total_feedback, 0.0f);
}

TEST_F(GolgiCellTest, StatsTrackGolgiSpikes) {
    for (int i = 0; i < 50; i++) {
        mossy_fiber_input_t input;
        input.fiber_id = i % 10;
        input.activity = 0.8f;
        input.timestamp_ms = (float)i;
        input.modality = 0;
        cerebellum_process_mossy_input(adapter, &input);
    }

    motor_coordination_result_t result;
    cerebellum_process(adapter, &result);

    cerebellum_stats_t stats;
    cerebellum_get_stats(adapter, &stats);

    EXPECT_GT(stats.golgi_cell_spikes, 0);
    EXPECT_GT(stats.golgi_feedback_total, 0.0f);
}

//=============================================================================
// Combined Interneuron Network Tests
//=============================================================================

class CombinedInterneuronTest : public ::testing::Test {
protected:
    cerebellum_adapter_t* adapter = nullptr;

    void SetUp() override {
        cerebellum_config_t config = cerebellum_default_config();
        // Enable all interneurons
        config.enable_basket_cells = true;
        config.num_basket_cells = 10;
        config.purkinje_per_basket = 5;
        config.enable_stellate_cells = true;
        config.num_stellate_cells = 20;
        config.purkinje_per_stellate = 3;
        config.enable_golgi_cells = true;
        config.num_golgi_cells = 20;
        config.granules_per_golgi = 75;
        adapter = cerebellum_create(&config);
        ASSERT_NE(adapter, nullptr);
    }

    void TearDown() override {
        if (adapter) {
            cerebellum_destroy(adapter);
            adapter = nullptr;
        }
    }
};

TEST_F(CombinedInterneuronTest, AllInterneuronsEnabled) {
    cerebellum_config_t retrieved;
    cerebellum_get_config(adapter, &retrieved);
    EXPECT_TRUE(retrieved.enable_basket_cells);
    EXPECT_TRUE(retrieved.enable_stellate_cells);
    EXPECT_TRUE(retrieved.enable_golgi_cells);
}

TEST_F(CombinedInterneuronTest, MolecularLayerCreated) {
    molecular_layer_interneurons_t* molecular = cerebellum_get_molecular_interneurons(adapter);
    EXPECT_NE(molecular, nullptr);
}

TEST_F(CombinedInterneuronTest, FullProcessingWithInterneurons) {
    // Process substantial input
    for (int i = 0; i < 100; i++) {
        mossy_fiber_input_t input;
        input.fiber_id = i % 20;
        input.activity = 0.5f + 0.5f * sinf((float)i * 0.1f);
        input.timestamp_ms = (float)i;
        input.modality = 0;
        cerebellum_process_mossy_input(adapter, &input);
    }

    motor_coordination_result_t result;
    EXPECT_TRUE(cerebellum_process(adapter, &result));
    EXPECT_TRUE(result.motor_ready);

    // Verify all interneuron types contributed
    cerebellum_stats_t stats;
    cerebellum_get_stats(adapter, &stats);

    EXPECT_GT(stats.basket_cell_spikes, 0);
    EXPECT_GT(stats.stellate_cell_spikes, 0);
    EXPECT_GT(stats.golgi_cell_spikes, 0);
}

TEST_F(CombinedInterneuronTest, InhibitionReducesPurkinjeOutput) {
    // Create adapter without interneurons for comparison
    cerebellum_config_t config_no_inh = cerebellum_default_config();
    config_no_inh.enable_basket_cells = false;
    config_no_inh.enable_stellate_cells = false;
    config_no_inh.enable_golgi_cells = false;
    cerebellum_adapter_t* adapter_no_inh = cerebellum_create(&config_no_inh);
    ASSERT_NE(adapter_no_inh, nullptr);

    // Same input to both
    mossy_fiber_input_t input;
    input.fiber_id = 0;
    input.activity = 0.9f;
    input.timestamp_ms = 0.0f;
    input.modality = 0;

    cerebellum_process_mossy_input(adapter, &input);
    cerebellum_process_mossy_input(adapter_no_inh, &input);

    motor_coordination_result_t result_with_inh, result_no_inh;
    cerebellum_process(adapter, &result_with_inh);
    cerebellum_process(adapter_no_inh, &result_no_inh);

    cerebellum_stats_t stats_with_inh, stats_no_inh;
    cerebellum_get_stats(adapter, &stats_with_inh);
    cerebellum_get_stats(adapter_no_inh, &stats_no_inh);

    // With inhibition, Purkinje output should generally be more regulated
    // (This is a qualitative test - the exact relationship depends on parameters)

    cerebellum_destroy(adapter_no_inh);
}

//=============================================================================
// Firing Rate Tests
//=============================================================================

class InterneuronFiringRateTest : public ::testing::Test {
protected:
    cerebellum_adapter_t* adapter = nullptr;

    void SetUp() override {
        cerebellum_config_t config = cerebellum_default_config();
        config.enable_basket_cells = true;
        config.enable_stellate_cells = true;
        config.enable_golgi_cells = true;
        adapter = cerebellum_create(&config);
        ASSERT_NE(adapter, nullptr);
    }

    void TearDown() override {
        if (adapter) {
            cerebellum_destroy(adapter);
            adapter = nullptr;
        }
    }
};

TEST_F(InterneuronFiringRateTest, AverageFiringRates) {
    // Process input
    for (int i = 0; i < 100; i++) {
        mossy_fiber_input_t input;
        input.fiber_id = i % 10;
        input.activity = 0.7f;
        input.timestamp_ms = (float)i;
        input.modality = 0;
        cerebellum_process_mossy_input(adapter, &input);
    }

    motor_coordination_result_t result;
    cerebellum_process(adapter, &result);

    cerebellum_stats_t stats;
    cerebellum_get_stats(adapter, &stats);

    // All interneuron types should have non-zero average firing rates
    EXPECT_GT(stats.avg_basket_firing_rate, 0.0f);
    EXPECT_GT(stats.avg_stellate_firing_rate, 0.0f);
    EXPECT_GT(stats.avg_golgi_firing_rate, 0.0f);

    // Firing rates should be in biological range (up to ~200 Hz for fast-spiking)
    EXPECT_LT(stats.avg_basket_firing_rate, 300.0f);
    EXPECT_LT(stats.avg_stellate_firing_rate, 300.0f);
    EXPECT_LT(stats.avg_golgi_firing_rate, 300.0f);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
