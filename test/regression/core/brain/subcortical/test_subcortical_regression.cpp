//=============================================================================
// test_subcortical_regression.cpp - Subcortical Brain Structures Regression Tests
//=============================================================================
/**
 * @file test_subcortical_regression.cpp
 * @brief Regression tests for subcortical brain structures
 *
 * WHAT: Stability and backward compatibility tests for basal ganglia components
 * WHY:  Ensure consistent behavior across code changes
 * HOW:  Test determinism, memory safety, numerical stability, and state consistency
 *
 * @author NIMCP Development Team
 * @date 2025-12-22
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <cstring>

// Headers have their own extern "C" guards
#include "core/brain/subcortical/nimcp_basal_ganglia.h"
#include "core/brain/subcortical/nimcp_striatum.h"
#include "core/brain/subcortical/nimcp_globus_pallidus.h"
#include "core/brain/subcortical/nimcp_substantia_nigra.h"
#include "core/brain/subcortical/nimcp_subthalamic.h"

//=============================================================================
// Test Fixture
//=============================================================================

class SubcorticalRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

//=============================================================================
// Determinism Tests - Same inputs must produce same outputs
//=============================================================================

TEST_F(SubcorticalRegressionTest, StriatumDeterminism) {
    // Create two identical striatum instances
    striatum_config_t config;
    striatum_default_config(&config);
    config.num_actions = 4;
    config.neurons_per_pathway = 100;

    striatum_t* s1 = striatum_create(&config);
    striatum_t* s2 = striatum_create(&config);
    ASSERT_NE(s1, nullptr);
    ASSERT_NE(s2, nullptr);

    // Apply identical inputs
    float cortical[4] = {0.5f, 0.3f, 0.8f, 0.2f};
    float dopamine = 0.6f;

    striatum_process_input(s1, cortical, dopamine);
    striatum_process_input(s2, cortical, dopamine);

    // Get outputs
    float d1_out1[4], d1_out2[4], d2_out1[4], d2_out2[4];
    striatum_get_d1_output(s1, d1_out1);
    striatum_get_d1_output(s2, d1_out2);
    striatum_get_d2_output(s1, d2_out1);
    striatum_get_d2_output(s2, d2_out2);

    // Must be identical
    for (int i = 0; i < 4; i++) {
        EXPECT_FLOAT_EQ(d1_out1[i], d1_out2[i]) << "D1 output mismatch at " << i;
        EXPECT_FLOAT_EQ(d2_out1[i], d2_out2[i]) << "D2 output mismatch at " << i;
    }

    striatum_destroy(s1);
    striatum_destroy(s2);
}

TEST_F(SubcorticalRegressionTest, GPDeterminism) {
    globus_pallidus_config_t config;
    globus_pallidus_default_config(&config, GP_SEGMENT_INTERNAL);
    config.num_actions = 4;

    globus_pallidus_t* gp1 = globus_pallidus_create(&config);
    globus_pallidus_t* gp2 = globus_pallidus_create(&config);
    ASSERT_NE(gp1, nullptr);
    ASSERT_NE(gp2, nullptr);

    float input[4] = {0.4f, 0.6f, 0.2f, 0.8f};
    globus_pallidus_set_striatal_input(gp1, input);
    globus_pallidus_set_striatal_input(gp2, input);
    globus_pallidus_process(gp1);
    globus_pallidus_process(gp2);

    float out1[4], out2[4];
    globus_pallidus_get_output(gp1, out1);
    globus_pallidus_get_output(gp2, out2);

    for (int i = 0; i < 4; i++) {
        EXPECT_FLOAT_EQ(out1[i], out2[i]) << "GP output mismatch at " << i;
    }

    globus_pallidus_destroy(gp1);
    globus_pallidus_destroy(gp2);
}

TEST_F(SubcorticalRegressionTest, STNDeterminism) {
    subthalamic_config_t config;
    subthalamic_default_config(&config);
    config.num_actions = 4;

    subthalamic_nucleus_t* stn1 = subthalamic_create(&config);
    subthalamic_nucleus_t* stn2 = subthalamic_create(&config);
    ASSERT_NE(stn1, nullptr);
    ASSERT_NE(stn2, nullptr);

    float cortical[4] = {0.9f, 0.1f, 0.5f, 0.3f};
    subthalamic_set_cortical_input(stn1, cortical, false);
    subthalamic_set_cortical_input(stn2, cortical, false);
    subthalamic_process(stn1);
    subthalamic_process(stn2);

    float out1[4], out2[4];
    subthalamic_get_output(stn1, out1);
    subthalamic_get_output(stn2, out2);

    for (int i = 0; i < 4; i++) {
        EXPECT_FLOAT_EQ(out1[i], out2[i]) << "STN output mismatch at " << i;
    }

    subthalamic_destroy(stn1);
    subthalamic_destroy(stn2);
}

TEST_F(SubcorticalRegressionTest, SNDeterminism) {
    substantia_nigra_config_t config;
    substantia_nigra_default_config(&config, SN_PART_COMPACTA);
    config.num_actions = 4;

    substantia_nigra_t* sn1 = substantia_nigra_create(&config);
    substantia_nigra_t* sn2 = substantia_nigra_create(&config);
    ASSERT_NE(sn1, nullptr);
    ASSERT_NE(sn2, nullptr);

    // Apply reward using snc API
    snc_update_reward(sn1, 1.0f, 0.5f);
    snc_update_reward(sn2, 1.0f, 0.5f);
    snc_step(sn1, 1.0f);
    snc_step(sn2, 1.0f);

    EXPECT_FLOAT_EQ(snc_get_dopamine(sn1), snc_get_dopamine(sn2));
    EXPECT_FLOAT_EQ(snc_get_rpe(sn1), snc_get_rpe(sn2));

    substantia_nigra_destroy(sn1);
    substantia_nigra_destroy(sn2);
}

TEST_F(SubcorticalRegressionTest, BasalGangliaDeterminism) {
    basal_ganglia_config_t config;
    basal_ganglia_default_config(&config);
    config.num_actions = 4;

    basal_ganglia_t* bg1 = basal_ganglia_create(&config);
    basal_ganglia_t* bg2 = basal_ganglia_create(&config);
    ASSERT_NE(bg1, nullptr);
    ASSERT_NE(bg2, nullptr);

    float cortical[4] = {0.2f, 0.8f, 0.4f, 0.6f};
    uint32_t action1, action2;
    basal_ganglia_select_action(bg1, cortical, &action1);
    basal_ganglia_select_action(bg2, cortical, &action2);

    float out1[4], out2[4];
    basal_ganglia_get_thalamic_output(bg1, out1);
    basal_ganglia_get_thalamic_output(bg2, out2);

    for (int i = 0; i < 4; i++) {
        EXPECT_FLOAT_EQ(out1[i], out2[i]) << "BG action value mismatch at " << i;
    }

    EXPECT_EQ(action1, action2);

    basal_ganglia_destroy(bg1);
    basal_ganglia_destroy(bg2);
}

//=============================================================================
// Memory Safety Tests - Create/destroy cycles must not leak
//=============================================================================

TEST_F(SubcorticalRegressionTest, StriatumCreateDestroyCycle) {
    striatum_config_t config;
    striatum_default_config(&config);

    // Many create/destroy cycles
    for (int i = 0; i < 100; i++) {
        striatum_t* s = striatum_create(&config);
        ASSERT_NE(s, nullptr) << "Failed at iteration " << i;
        striatum_destroy(s);
    }
}

TEST_F(SubcorticalRegressionTest, GPCreateDestroyCycle) {
    globus_pallidus_config_t config;
    globus_pallidus_default_config(&config, GP_SEGMENT_EXTERNAL);

    for (int i = 0; i < 100; i++) {
        globus_pallidus_t* gp = globus_pallidus_create(&config);
        ASSERT_NE(gp, nullptr) << "Failed at iteration " << i;
        globus_pallidus_destroy(gp);
    }
}

TEST_F(SubcorticalRegressionTest, STNCreateDestroyCycle) {
    subthalamic_config_t config;
    subthalamic_default_config(&config);

    for (int i = 0; i < 100; i++) {
        subthalamic_nucleus_t* stn = subthalamic_create(&config);
        ASSERT_NE(stn, nullptr) << "Failed at iteration " << i;
        subthalamic_destroy(stn);
    }
}

TEST_F(SubcorticalRegressionTest, SNCreateDestroyCycle) {
    substantia_nigra_config_t config;
    substantia_nigra_default_config(&config, SN_PART_COMPACTA);

    for (int i = 0; i < 100; i++) {
        substantia_nigra_t* sn = substantia_nigra_create(&config);
        ASSERT_NE(sn, nullptr) << "Failed at iteration " << i;
        substantia_nigra_destroy(sn);
    }
}

TEST_F(SubcorticalRegressionTest, BasalGangliaCreateDestroyCycle) {
    basal_ganglia_config_t config;
    basal_ganglia_default_config(&config);
    config.num_actions = 4;

    for (int i = 0; i < 50; i++) {  // Fewer iterations due to more allocations
        basal_ganglia_t* bg = basal_ganglia_create(&config);
        ASSERT_NE(bg, nullptr) << "Failed at iteration " << i;
        basal_ganglia_destroy(bg);
    }
}

//=============================================================================
// Numerical Stability Tests - No NaN/Inf under extreme inputs
//=============================================================================

TEST_F(SubcorticalRegressionTest, StriatumNumericalStability) {
    striatum_config_t config;
    striatum_default_config(&config);
    config.num_actions = 4;

    striatum_t* s = striatum_create(&config);
    ASSERT_NE(s, nullptr);

    // Test with extreme values
    float extreme_high[4] = {1000.0f, 1000.0f, 1000.0f, 1000.0f};
    float extreme_low[4] = {-1000.0f, -1000.0f, -1000.0f, -1000.0f};
    float output[4];

    // High values
    striatum_process_input(s, extreme_high, 100.0f);
    striatum_get_d1_output(s, output);
    for (int i = 0; i < 4; i++) {
        EXPECT_FALSE(std::isnan(output[i])) << "NaN at " << i;
        EXPECT_FALSE(std::isinf(output[i])) << "Inf at " << i;
    }

    // Reset and try low values
    striatum_reset(s);
    striatum_process_input(s, extreme_low, -100.0f);
    striatum_get_d1_output(s, output);
    for (int i = 0; i < 4; i++) {
        EXPECT_FALSE(std::isnan(output[i])) << "NaN at " << i;
        EXPECT_FALSE(std::isinf(output[i])) << "Inf at " << i;
    }

    striatum_destroy(s);
}

TEST_F(SubcorticalRegressionTest, GPNumericalStability) {
    globus_pallidus_config_t config;
    globus_pallidus_default_config(&config, GP_SEGMENT_INTERNAL);
    config.num_actions = 4;

    globus_pallidus_t* gp = globus_pallidus_create(&config);
    ASSERT_NE(gp, nullptr);

    float extreme[4] = {1e10f, -1e10f, 1e10f, -1e10f};
    float output[4];

    globus_pallidus_set_striatal_input(gp, extreme);
    globus_pallidus_process(gp);
    globus_pallidus_get_output(gp, output);

    for (int i = 0; i < 4; i++) {
        EXPECT_FALSE(std::isnan(output[i])) << "NaN at " << i;
        EXPECT_FALSE(std::isinf(output[i])) << "Inf at " << i;
    }

    globus_pallidus_destroy(gp);
}

TEST_F(SubcorticalRegressionTest, STNNumericalStability) {
    subthalamic_config_t config;
    subthalamic_default_config(&config);
    config.num_actions = 4;

    subthalamic_nucleus_t* stn = subthalamic_create(&config);
    ASSERT_NE(stn, nullptr);

    float extreme[4] = {1e20f, 1e20f, 1e20f, 1e20f};
    float output[4];

    subthalamic_set_cortical_input(stn, extreme, false);
    subthalamic_process(stn);
    subthalamic_get_output(stn, output);

    for (int i = 0; i < 4; i++) {
        EXPECT_FALSE(std::isnan(output[i])) << "NaN at " << i;
        EXPECT_FALSE(std::isinf(output[i])) << "Inf at " << i;
    }

    subthalamic_destroy(stn);
}

TEST_F(SubcorticalRegressionTest, SNNumericalStability) {
    substantia_nigra_config_t config;
    substantia_nigra_default_config(&config, SN_PART_COMPACTA);

    substantia_nigra_t* sn = substantia_nigra_create(&config);
    ASSERT_NE(sn, nullptr);

    // Extreme reward values
    snc_update_reward(sn, 1e30f, 0.0f);
    snc_step(sn, 1.0f);
    float dopamine = snc_get_dopamine(sn);
    EXPECT_FALSE(std::isnan(dopamine));
    EXPECT_FALSE(std::isinf(dopamine));

    substantia_nigra_reset(sn);
    snc_update_reward(sn, -1e30f, 0.0f);
    snc_step(sn, 1.0f);
    dopamine = snc_get_dopamine(sn);
    EXPECT_FALSE(std::isnan(dopamine));
    EXPECT_FALSE(std::isinf(dopamine));

    substantia_nigra_destroy(sn);
}

//=============================================================================
// State Consistency Tests - Reset must restore initial state
//=============================================================================

TEST_F(SubcorticalRegressionTest, StriatumResetConsistency) {
    striatum_config_t config;
    striatum_default_config(&config);
    config.num_actions = 4;

    striatum_t* s = striatum_create(&config);
    ASSERT_NE(s, nullptr);

    // Get initial output
    float initial_d1[4], initial_d2[4];
    striatum_get_d1_output(s, initial_d1);
    striatum_get_d2_output(s, initial_d2);

    // Process with some input
    float input[4] = {0.9f, 0.9f, 0.9f, 0.9f};
    striatum_process_input(s, input, 1.0f);

    // Reset
    striatum_reset(s);

    // Get output after reset
    float reset_d1[4], reset_d2[4];
    striatum_get_d1_output(s, reset_d1);
    striatum_get_d2_output(s, reset_d2);

    // Should match initial
    for (int i = 0; i < 4; i++) {
        EXPECT_FLOAT_EQ(initial_d1[i], reset_d1[i]) << "D1 reset mismatch at " << i;
        EXPECT_FLOAT_EQ(initial_d2[i], reset_d2[i]) << "D2 reset mismatch at " << i;
    }

    striatum_destroy(s);
}

TEST_F(SubcorticalRegressionTest, GPResetConsistency) {
    globus_pallidus_config_t config;
    globus_pallidus_default_config(&config, GP_SEGMENT_INTERNAL);
    config.num_actions = 4;

    globus_pallidus_t* gp = globus_pallidus_create(&config);
    ASSERT_NE(gp, nullptr);

    float initial[4];
    globus_pallidus_get_output(gp, initial);

    float input[4] = {0.8f, 0.8f, 0.8f, 0.8f};
    globus_pallidus_set_striatal_input(gp, input);
    globus_pallidus_process(gp);

    globus_pallidus_reset(gp);

    float reset[4];
    globus_pallidus_get_output(gp, reset);

    for (int i = 0; i < 4; i++) {
        EXPECT_FLOAT_EQ(initial[i], reset[i]) << "GP reset mismatch at " << i;
    }

    globus_pallidus_destroy(gp);
}

TEST_F(SubcorticalRegressionTest, STNResetConsistency) {
    subthalamic_config_t config;
    subthalamic_default_config(&config);
    config.num_actions = 4;

    subthalamic_nucleus_t* stn = subthalamic_create(&config);
    ASSERT_NE(stn, nullptr);

    float initial = subthalamic_get_global_output(stn);
    stn_mode_t initial_mode = subthalamic_get_mode(stn);

    // Emergency stop changes mode and output
    subthalamic_emergency_stop(stn, 1.0f);
    EXPECT_NE(subthalamic_get_mode(stn), initial_mode);

    // Reset
    subthalamic_reset(stn);

    EXPECT_FLOAT_EQ(initial, subthalamic_get_global_output(stn));
    EXPECT_EQ(initial_mode, subthalamic_get_mode(stn));

    subthalamic_destroy(stn);
}

TEST_F(SubcorticalRegressionTest, SNResetConsistency) {
    substantia_nigra_config_t config;
    substantia_nigra_default_config(&config, SN_PART_COMPACTA);

    substantia_nigra_t* sn = substantia_nigra_create(&config);
    ASSERT_NE(sn, nullptr);

    float initial_da = snc_get_dopamine(sn);
    float initial_rpe = snc_get_rpe(sn);

    snc_update_reward(sn, 5.0f, 0.0f);
    snc_step(sn, 1.0f);

    substantia_nigra_reset(sn);

    EXPECT_FLOAT_EQ(initial_da, snc_get_dopamine(sn));
    EXPECT_FLOAT_EQ(initial_rpe, snc_get_rpe(sn));

    substantia_nigra_destroy(sn);
}

TEST_F(SubcorticalRegressionTest, BasalGangliaResetConsistency) {
    basal_ganglia_config_t config;
    basal_ganglia_default_config(&config);
    config.num_actions = 4;

    basal_ganglia_t* bg = basal_ganglia_create(&config);
    ASSERT_NE(bg, nullptr);

    float initial[4];
    basal_ganglia_get_thalamic_output(bg, initial);

    float input[4] = {0.5f, 0.7f, 0.3f, 0.9f};
    uint32_t selected;
    basal_ganglia_select_action(bg, input, &selected);
    basal_ganglia_update_dopamine(bg, 1.0f, 0.5f);
    basal_ganglia_step(bg, 1.0f);

    basal_ganglia_reset(bg);

    float reset[4];
    basal_ganglia_get_thalamic_output(bg, reset);

    for (int i = 0; i < 4; i++) {
        EXPECT_FLOAT_EQ(initial[i], reset[i]) << "BG reset mismatch at " << i;
    }

    basal_ganglia_destroy(bg);
}

//=============================================================================
// Output Range Tests - Outputs must stay within expected bounds
//=============================================================================

TEST_F(SubcorticalRegressionTest, StriatumOutputBounds) {
    striatum_config_t config;
    striatum_default_config(&config);
    config.num_actions = 4;

    striatum_t* s = striatum_create(&config);
    ASSERT_NE(s, nullptr);

    // Various input combinations
    float inputs[][4] = {
        {0.0f, 0.0f, 0.0f, 0.0f},
        {1.0f, 1.0f, 1.0f, 1.0f},
        {0.5f, 0.5f, 0.5f, 0.5f},
        {0.0f, 1.0f, 0.0f, 1.0f}
    };
    float dopamine_levels[] = {0.0f, 0.5f, 1.0f, 2.0f};

    float d1[4], d2[4];
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            striatum_reset(s);
            striatum_process_input(s, inputs[i], dopamine_levels[j]);
            striatum_get_d1_output(s, d1);
            striatum_get_d2_output(s, d2);

            for (int k = 0; k < 4; k++) {
                EXPECT_GE(d1[k], 0.0f) << "D1 < 0 at " << k;
                EXPECT_LE(d1[k], 1.0f) << "D1 > 1 at " << k;
                EXPECT_GE(d2[k], 0.0f) << "D2 < 0 at " << k;
                EXPECT_LE(d2[k], 1.0f) << "D2 > 1 at " << k;
            }
        }
    }

    striatum_destroy(s);
}

TEST_F(SubcorticalRegressionTest, GPOutputBounds) {
    globus_pallidus_config_t config;
    globus_pallidus_default_config(&config, GP_SEGMENT_INTERNAL);
    config.num_actions = 4;

    globus_pallidus_t* gp = globus_pallidus_create(&config);
    ASSERT_NE(gp, nullptr);

    float inputs[][4] = {
        {0.0f, 0.0f, 0.0f, 0.0f},
        {1.0f, 1.0f, 1.0f, 1.0f},
        {0.5f, 0.5f, 0.5f, 0.5f}
    };

    float out[4];
    for (int i = 0; i < 3; i++) {
        globus_pallidus_reset(gp);
        globus_pallidus_set_striatal_input(gp, inputs[i]);
        globus_pallidus_process(gp);
        globus_pallidus_get_output(gp, out);

        for (int k = 0; k < 4; k++) {
            EXPECT_GE(out[k], 0.0f) << "GP < 0 at " << k;
            EXPECT_LE(out[k], 1.0f) << "GP > 1 at " << k;
        }
    }

    globus_pallidus_destroy(gp);
}

TEST_F(SubcorticalRegressionTest, STNOutputBounds) {
    subthalamic_config_t config;
    subthalamic_default_config(&config);
    config.num_actions = 4;

    subthalamic_nucleus_t* stn = subthalamic_create(&config);
    ASSERT_NE(stn, nullptr);

    float inputs[][4] = {
        {0.0f, 0.0f, 0.0f, 0.0f},
        {1.0f, 1.0f, 1.0f, 1.0f},
        {0.5f, 0.5f, 0.5f, 0.5f}
    };

    float out[4];
    for (int i = 0; i < 3; i++) {
        subthalamic_reset(stn);
        subthalamic_set_cortical_input(stn, inputs[i], false);
        subthalamic_process(stn);
        subthalamic_get_output(stn, out);

        for (int k = 0; k < 4; k++) {
            EXPECT_GE(out[k], 0.0f) << "STN < 0 at " << k;
            EXPECT_LE(out[k], 1.0f) << "STN > 1 at " << k;
        }
    }

    subthalamic_destroy(stn);
}

TEST_F(SubcorticalRegressionTest, SNOutputBounds) {
    substantia_nigra_config_t config;
    substantia_nigra_default_config(&config, SN_PART_COMPACTA);

    substantia_nigra_t* sn = substantia_nigra_create(&config);
    ASSERT_NE(sn, nullptr);

    float rewards[] = {-1.0f, 0.0f, 0.5f, 1.0f, 2.0f};

    for (int i = 0; i < 5; i++) {
        substantia_nigra_reset(sn);
        snc_update_reward(sn, rewards[i], 0.0f);
        snc_step(sn, 1.0f);

        float da = snc_get_dopamine(sn);
        EXPECT_GE(da, 0.0f) << "DA < 0";
        EXPECT_LE(da, 1.5f) << "DA > reasonable max";
    }

    substantia_nigra_destroy(sn);
}

//=============================================================================
// Long-Running Stability Tests
//=============================================================================

TEST_F(SubcorticalRegressionTest, StriatumLongRunStability) {
    striatum_config_t config;
    striatum_default_config(&config);
    config.num_actions = 4;

    striatum_t* s = striatum_create(&config);
    ASSERT_NE(s, nullptr);

    float input[4] = {0.5f, 0.5f, 0.5f, 0.5f};
    float output[4];

    // Run 1000 iterations
    for (int i = 0; i < 1000; i++) {
        striatum_process_input(s, input, 0.5f);
        striatum_get_d1_output(s, output);

        for (int k = 0; k < 4; k++) {
            EXPECT_FALSE(std::isnan(output[k])) << "NaN at iter " << i;
            EXPECT_FALSE(std::isinf(output[k])) << "Inf at iter " << i;
        }
    }

    striatum_destroy(s);
}

TEST_F(SubcorticalRegressionTest, BasalGangliaLongRunStability) {
    basal_ganglia_config_t config;
    basal_ganglia_default_config(&config);
    config.num_actions = 4;

    basal_ganglia_t* bg = basal_ganglia_create(&config);
    ASSERT_NE(bg, nullptr);

    float input[4] = {0.5f, 0.5f, 0.5f, 0.5f};
    float output[4];

    // Run 500 iterations
    for (int i = 0; i < 500; i++) {
        uint32_t selected;
        basal_ganglia_select_action(bg, input, &selected);
        basal_ganglia_update_dopamine(bg, (i % 2 == 0) ? 0.5f : -0.5f, 0.0f);
        basal_ganglia_step(bg, 1.0f);
        basal_ganglia_get_thalamic_output(bg, output);

        for (int k = 0; k < 4; k++) {
            EXPECT_FALSE(std::isnan(output[k])) << "NaN at iter " << i;
            EXPECT_FALSE(std::isinf(output[k])) << "Inf at iter " << i;
        }
    }

    basal_ganglia_destroy(bg);
}

//=============================================================================
// Backward Compatibility Tests - Default configs must work
//=============================================================================

TEST_F(SubcorticalRegressionTest, DefaultConfigsWork) {
    // Striatum
    striatum_config_t str_cfg;
    striatum_default_config(&str_cfg);
    striatum_t* str = striatum_create(&str_cfg);
    EXPECT_NE(str, nullptr);
    striatum_destroy(str);

    // GPe
    globus_pallidus_config_t gpe_cfg;
    globus_pallidus_default_config(&gpe_cfg, GP_SEGMENT_EXTERNAL);
    globus_pallidus_t* gpe = globus_pallidus_create(&gpe_cfg);
    EXPECT_NE(gpe, nullptr);
    globus_pallidus_destroy(gpe);

    // GPi
    globus_pallidus_config_t gpi_cfg;
    globus_pallidus_default_config(&gpi_cfg, GP_SEGMENT_INTERNAL);
    globus_pallidus_t* gpi = globus_pallidus_create(&gpi_cfg);
    EXPECT_NE(gpi, nullptr);
    globus_pallidus_destroy(gpi);

    // STN
    subthalamic_config_t stn_cfg;
    subthalamic_default_config(&stn_cfg);
    subthalamic_nucleus_t* stn = subthalamic_create(&stn_cfg);
    EXPECT_NE(stn, nullptr);
    subthalamic_destroy(stn);

    // SNc
    substantia_nigra_config_t snc_cfg;
    substantia_nigra_default_config(&snc_cfg, SN_PART_COMPACTA);
    substantia_nigra_t* snc = substantia_nigra_create(&snc_cfg);
    EXPECT_NE(snc, nullptr);
    substantia_nigra_destroy(snc);

    // SNr
    substantia_nigra_config_t snr_cfg;
    substantia_nigra_default_config(&snr_cfg, SN_PART_RETICULATA);
    substantia_nigra_t* snr = substantia_nigra_create(&snr_cfg);
    EXPECT_NE(snr, nullptr);
    substantia_nigra_destroy(snr);

    // Full BG
    basal_ganglia_config_t bg_cfg;
    basal_ganglia_default_config(&bg_cfg);
    basal_ganglia_t* bg = basal_ganglia_create(&bg_cfg);
    EXPECT_NE(bg, nullptr);
    basal_ganglia_destroy(bg);
}

TEST_F(SubcorticalRegressionTest, NullInputHandling) {
    // All functions should handle NULL instance gracefully
    // Note: Some create() functions accept NULL config and use defaults

    // Striatum - NULL config uses defaults (valid)
    striatum_t* str = striatum_create(nullptr);
    if (str) striatum_destroy(str);  // Clean up if created
    striatum_destroy(nullptr);  // Should not crash
    EXPECT_EQ(striatum_reset(nullptr), -1);
    EXPECT_EQ(striatum_process_input(nullptr, nullptr, 0.5f), -1);
    EXPECT_EQ(striatum_set_dopamine(nullptr, 0.5f), -1);

    // GP - NULL config returns NULL
    EXPECT_EQ(globus_pallidus_create(nullptr), nullptr);
    globus_pallidus_destroy(nullptr);
    EXPECT_EQ(globus_pallidus_reset(nullptr), -1);
    EXPECT_EQ(globus_pallidus_process(nullptr), -1);

    // STN - NULL config returns NULL
    EXPECT_EQ(subthalamic_create(nullptr), nullptr);
    subthalamic_destroy(nullptr);
    EXPECT_EQ(subthalamic_reset(nullptr), -1);
    EXPECT_EQ(subthalamic_process(nullptr), -1);

    // SN - NULL config returns NULL
    EXPECT_EQ(substantia_nigra_create(nullptr), nullptr);
    substantia_nigra_destroy(nullptr);
    EXPECT_EQ(substantia_nigra_reset(nullptr), -1);
    EXPECT_EQ(snc_step(nullptr, 1.0f), -1);

    // BG - NULL config uses defaults
    basal_ganglia_t* bg = basal_ganglia_create(nullptr);
    if (bg) basal_ganglia_destroy(bg);  // Clean up if created
    basal_ganglia_destroy(nullptr);
    EXPECT_EQ(basal_ganglia_reset(nullptr), -1);
    EXPECT_EQ(basal_ganglia_step(nullptr, 1.0f), -1);
}
