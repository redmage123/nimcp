/**
 * @file test_cortical_dendritic.cpp
 * @brief Unit tests for dendritic computation in cortical columns
 */

#include <gtest/gtest.h>
// Headers have their own extern "C" guards
#include "core/cortical_columns/nimcp_cortical_dendritic.h"

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class CorticalDendriticTest : public ::testing::Test {
protected:
    cortical_dendritic_t* dend;
    dendritic_config_t config;
    static const int NUM_CELLS = 32;

    void SetUp() override {
        cortical_dendritic_default_config(&config);
        dend = cortical_dendritic_create(&config, NUM_CELLS);
        ASSERT_NE(dend, nullptr);
    }

    void TearDown() override {
        if (dend) {
            cortical_dendritic_destroy(dend);
        }
    }

    /* Helper to set all basal inputs */
    void set_all_basal(float value) {
        for (int i = 0; i < NUM_CELLS; i++) {
            cortical_dendritic_set_basal_input(dend, i, value);
        }
    }

    /* Helper to set all apical inputs */
    void set_all_apical(float value) {
        for (int i = 0; i < NUM_CELLS; i++) {
            cortical_dendritic_set_apical_input(dend, i, value);
        }
    }
};

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

TEST_F(CorticalDendriticTest, DefaultConfig) {
    dendritic_config_t cfg;
    int result = cortical_dendritic_default_config(&cfg);

    EXPECT_EQ(result, 0);
    EXPECT_GT(cfg.basal_weight, 0.0f);
    EXPECT_GT(cfg.apical_weight, 0.0f);
    EXPECT_GT(cfg.burst_threshold, 0.0f);
}

TEST_F(CorticalDendriticTest, DefaultConfigNullPointer) {
    int result = cortical_dendritic_default_config(nullptr);
    EXPECT_LT(result, 0);
}

TEST_F(CorticalDendriticTest, CreateWithConfig) {
    dendritic_config_t custom_config;
    cortical_dendritic_default_config(&custom_config);
    custom_config.basal_weight = 0.7f;
    custom_config.apical_weight = 0.3f;

    cortical_dendritic_t* system = cortical_dendritic_create(&custom_config, 64);
    ASSERT_NE(system, nullptr);

    cortical_dendritic_destroy(system);
}

TEST_F(CorticalDendriticTest, CreateWithNullConfig) {
    cortical_dendritic_t* system = cortical_dendritic_create(nullptr, 32);
    ASSERT_NE(system, nullptr);
    cortical_dendritic_destroy(system);
}

/* ============================================================================
 * Input Setting Tests
 * ============================================================================ */

TEST_F(CorticalDendriticTest, SetBasalInput) {
    /* API: set_basal_input(dend, cell_idx, input) */
    int result = cortical_dendritic_set_basal_input(dend, 0, 0.5f);
    EXPECT_EQ(result, 0);
}

TEST_F(CorticalDendriticTest, SetApicalInput) {
    int result = cortical_dendritic_set_apical_input(dend, 0, 0.6f);
    EXPECT_EQ(result, 0);
}

TEST_F(CorticalDendriticTest, SetObliqueInput) {
    int result = cortical_dendritic_set_oblique_input(dend, 0, 0.4f);
    EXPECT_EQ(result, 0);
}

TEST_F(CorticalDendriticTest, SetBasalInputAllCells) {
    for (int i = 0; i < NUM_CELLS; i++) {
        int result = cortical_dendritic_set_basal_input(dend, i, (float)i / NUM_CELLS);
        EXPECT_EQ(result, 0);
    }
}

TEST_F(CorticalDendriticTest, SetInputInvalidIndex) {
    int result = cortical_dendritic_set_basal_input(dend, 10000, 0.5f);
    EXPECT_LT(result, 0);
}

/* ============================================================================
 * Update Tests
 * ============================================================================ */

TEST_F(CorticalDendriticTest, Update) {
    float dt = 1.0f;  /* 1 ms timestep */
    int result = cortical_dendritic_update(dend, dt);
    EXPECT_EQ(result, 0);
}

TEST_F(CorticalDendriticTest, UpdateWithInput) {
    /* Set inputs for all cells */
    set_all_basal(0.8f);
    set_all_apical(0.7f);

    /* Update */
    int result = cortical_dendritic_update(dend, 1.0f);
    EXPECT_EQ(result, 0);
}

TEST_F(CorticalDendriticTest, MultipleUpdates) {
    for (int i = 0; i < 100; i++) {
        int result = cortical_dendritic_update(dend, 1.0f);
        EXPECT_EQ(result, 0);
    }
}

/* ============================================================================
 * Output Query Tests
 * ============================================================================ */

TEST_F(CorticalDendriticTest, GetOutputMode) {
    cortical_dendritic_update(dend, 1.0f);

    output_mode_t mode;
    int result = cortical_dendritic_get_output_mode(dend, 0, &mode);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(mode == OUTPUT_SILENT || mode == OUTPUT_SINGLE_SPIKE || mode == OUTPUT_BURST);
}

TEST_F(CorticalDendriticTest, GetSomaVoltage) {
    cortical_dendritic_update(dend, 1.0f);

    float voltage;
    int result = cortical_dendritic_get_soma_voltage(dend, 0, &voltage);
    EXPECT_EQ(result, 0);
}

TEST_F(CorticalDendriticTest, IsCalciumSpikeActive) {
    /* Provide strong apical input to potentially trigger Ca spike */
    set_all_apical(1.0f);
    cortical_dendritic_update(dend, 1.0f);

    bool active;
    int result = cortical_dendritic_is_calcium_spike_active(dend, 0, &active);
    EXPECT_EQ(result, 0);
    /* Just verify the call works */
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(CorticalDendriticTest, GetStats) {
    dendritic_stats_t stats;
    int result = cortical_dendritic_get_stats(dend, &stats);
    EXPECT_EQ(result, 0);
}

TEST_F(CorticalDendriticTest, ResetStats) {
    cortical_dendritic_update(dend, 1.0f);
    int result = cortical_dendritic_reset_stats(dend);
    EXPECT_EQ(result, 0);

    dendritic_stats_t stats;
    cortical_dendritic_get_stats(dend, &stats);
    EXPECT_EQ(stats.total_updates, 0u);
}

/* ============================================================================
 * Bio-async Tests
 * ============================================================================ */

TEST_F(CorticalDendriticTest, ConnectBioAsync) {
    int result = cortical_dendritic_connect_bio_async(dend);
    /* Bio-async may return various codes depending on router state */
    (void)result; /* Just verify no crash */
}

TEST_F(CorticalDendriticTest, IsBioAsyncConnected) {
    bool connected = cortical_dendritic_is_bio_async_connected(dend);
    EXPECT_FALSE(connected);
}

TEST_F(CorticalDendriticTest, DisconnectBioAsync) {
    int result = cortical_dendritic_disconnect_bio_async(dend);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Edge Cases
 * ============================================================================ */

TEST_F(CorticalDendriticTest, DestroyNull) {
    cortical_dendritic_destroy(nullptr);
}

TEST_F(CorticalDendriticTest, ZeroInput) {
    set_all_basal(0.0f);
    cortical_dendritic_update(dend, 1.0f);

    output_mode_t mode;
    cortical_dendritic_get_output_mode(dend, 0, &mode);
    /* With zero input, should be silent or low activity */
    EXPECT_TRUE(mode == OUTPUT_SILENT || mode == OUTPUT_SINGLE_SPIKE || mode == OUTPUT_BURST);
}

TEST_F(CorticalDendriticTest, SaturatedInput) {
    set_all_basal(1.0f);
    set_all_apical(1.0f);
    cortical_dendritic_update(dend, 1.0f);

    output_mode_t mode;
    int result = cortical_dendritic_get_output_mode(dend, 0, &mode);
    EXPECT_EQ(result, 0);
}

TEST_F(CorticalDendriticTest, InvalidCellIndex) {
    output_mode_t mode;
    int result = cortical_dendritic_get_output_mode(dend, 10000, &mode);
    EXPECT_LT(result, 0);
}

TEST_F(CorticalDendriticTest, CoincidentInput) {
    /* Test BAC firing - coincident basal and apical input */
    set_all_basal(0.8f);
    set_all_apical(0.8f);

    /* Multiple updates to allow coincidence window */
    for (int i = 0; i < 10; i++) {
        cortical_dendritic_update(dend, 1.0f);
    }

    output_mode_t mode;
    int result = cortical_dendritic_get_output_mode(dend, 0, &mode);
    EXPECT_EQ(result, 0);
    /* With strong coincident input, might see bursts */
}

