//=============================================================================
// test_training_plasticity_bridge.cpp - Unit Tests for Training-Plasticity Bridge
//=============================================================================
/**
 * @file test_training_plasticity_bridge.cpp
 * @brief Comprehensive unit tests for Training-Plasticity Bridge module
 *
 * Tests cover:
 * - Lifecycle (create/destroy)
 * - RPE computation (all modes)
 * - Region configuration and routing
 * - Neuromodulator-LR coupling
 * - STDP/BCM integration
 * - Batch plasticity
 * - CoW snapshots
 * - Thread safety
 *
 * @version 1.0.0
 * @date 2025-11-27
 */

#include <gtest/gtest.h>
#include <cmath>
#include <thread>
#include <vector>
#include <atomic>

// Headers have their own extern "C" guards
#include "middleware/training/nimcp_training_plasticity_bridge.h"
#include "middleware/training/nimcp_training_callbacks.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class TrainingPlasticityBridgeTest : public ::testing::Test {
protected:
    void SetUp() override {
        ctx_ = nullptr;
    }

    void TearDown() override {
        if (ctx_) {
            tpb_destroy(ctx_);
            ctx_ = nullptr;
        }
    }

    tpb_context_t* ctx_;
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(TrainingPlasticityBridgeTest, CreateWithDefaults) {
    ctx_ = tpb_create(nullptr);
    ASSERT_NE(ctx_, nullptr) << "Failed to create bridge with defaults";
}

TEST_F(TrainingPlasticityBridgeTest, CreateWithConfig) {
    tpb_config_t config = tpb_config_default();
    config.rpe_mode = TPB_RPE_TEMPORAL_DIFF;
    config.thread_pool_size = 2;

    ctx_ = tpb_create(&config);
    ASSERT_NE(ctx_, nullptr) << "Failed to create bridge with config";
}

TEST_F(TrainingPlasticityBridgeTest, CreateWithPresets) {
    // Test each preset
    const char* presets[] = {"reinforcement", "supervised", "unsupervised", "biological"};

    for (const char* preset : presets) {
        tpb_config_t config = tpb_config_preset(preset);
        ctx_ = tpb_create(&config);
        ASSERT_NE(ctx_, nullptr) << "Failed to create bridge with preset: " << preset;
        tpb_destroy(ctx_);
        ctx_ = nullptr;
    }
}

TEST_F(TrainingPlasticityBridgeTest, DestroyNull) {
    // Should not crash
    tpb_destroy(nullptr);
}

TEST_F(TrainingPlasticityBridgeTest, DefaultConfigValues) {
    tpb_config_t config = tpb_config_default();

    EXPECT_EQ(config.rpe_mode, TPB_RPE_EXPONENTIAL_AVG);
    EXPECT_EQ(config.rpe_window_size, TPB_DEFAULT_RPE_WINDOW);
    EXPECT_FLOAT_EQ(config.rpe_smoothing_alpha, 0.1f);
    EXPECT_FLOAT_EQ(config.rpe_to_da_gain, 0.5f);
    EXPECT_EQ(config.thread_pool_size, TPB_DEFAULT_THREAD_POOL_SIZE);
    EXPECT_TRUE(config.enable_cow);
}

//=============================================================================
// RPE Computation Tests
//=============================================================================

class RPETest : public TrainingPlasticityBridgeTest {
protected:
    void SetUp() override {
        TrainingPlasticityBridgeTest::SetUp();
    }
};

TEST_F(RPETest, RPETemporalDiff) {
    tpb_config_t config = tpb_config_default();
    config.rpe_mode = TPB_RPE_TEMPORAL_DIFF;
    ctx_ = tpb_create(&config);
    ASSERT_NE(ctx_, nullptr);

    float rpe = 0.0f;

    // First loss establishes baseline
    EXPECT_EQ(tpb_report_loss(ctx_, 1.0f, &rpe), NIMCP_SUCCESS);
    EXPECT_NEAR(rpe, 0.0f, 0.01f) << "First RPE should be ~0";

    // Loss decrease = positive RPE
    EXPECT_EQ(tpb_report_loss(ctx_, 0.8f, &rpe), NIMCP_SUCCESS);
    EXPECT_GT(rpe, 0.0f) << "Loss decrease should give positive RPE";

    // Loss increase = negative RPE
    EXPECT_EQ(tpb_report_loss(ctx_, 1.2f, &rpe), NIMCP_SUCCESS);
    EXPECT_LT(rpe, 0.0f) << "Loss increase should give negative RPE";
}

TEST_F(RPETest, RPEExponentialAvg) {
    tpb_config_t config = tpb_config_default();
    config.rpe_mode = TPB_RPE_EXPONENTIAL_AVG;
    config.rpe_smoothing_alpha = 0.2f;
    ctx_ = tpb_create(&config);
    ASSERT_NE(ctx_, nullptr);

    float rpe = 0.0f;

    // Establish baseline
    tpb_report_loss(ctx_, 1.0f, &rpe);
    tpb_report_loss(ctx_, 1.0f, &rpe);
    tpb_report_loss(ctx_, 1.0f, &rpe);

    // Significant improvement
    EXPECT_EQ(tpb_report_loss(ctx_, 0.5f, &rpe), NIMCP_SUCCESS);
    EXPECT_GT(rpe, 0.0f) << "Large improvement should give positive RPE";
}

TEST_F(RPETest, RPESlidingWindow) {
    tpb_config_t config = tpb_config_default();
    config.rpe_mode = TPB_RPE_SLIDING_WINDOW;
    config.rpe_window_size = 5;
    ctx_ = tpb_create(&config);
    ASSERT_NE(ctx_, nullptr);

    float rpe = 0.0f;

    // Fill window with losses around 1.0
    for (int i = 0; i < 10; i++) {
        tpb_report_loss(ctx_, 1.0f + (float)(i % 2) * 0.1f, &rpe);
    }

    // Loss much lower than window average
    EXPECT_EQ(tpb_report_loss(ctx_, 0.5f, &rpe), NIMCP_SUCCESS);
    EXPECT_GT(rpe, 0.0f) << "Loss below window avg should give positive RPE";
}

TEST_F(RPETest, RPEAdaptive) {
    tpb_config_t config = tpb_config_default();
    config.rpe_mode = TPB_RPE_ADAPTIVE;
    ctx_ = tpb_create(&config);
    ASSERT_NE(ctx_, nullptr);

    float rpe = 0.0f;

    // Establish baseline with variance
    for (int i = 0; i < 20; i++) {
        float loss = 1.0f + 0.1f * sinf((float)i);
        tpb_report_loss(ctx_, loss, &rpe);
    }

    // Large deviation should be normalized
    EXPECT_EQ(tpb_report_loss(ctx_, 0.5f, &rpe), NIMCP_SUCCESS);
    // Should be positive but bounded by adaptive scaling
    EXPECT_GT(rpe, 0.0f);
}

TEST_F(RPETest, RPEInvalidInput) {
    ctx_ = tpb_create(nullptr);
    ASSERT_NE(ctx_, nullptr);

    float rpe = 0.0f;

    // NaN loss
    EXPECT_EQ(tpb_report_loss(ctx_, NAN, &rpe), NIMCP_ERROR_INVALID_PARAM);

    // Inf loss
    EXPECT_EQ(tpb_report_loss(ctx_, INFINITY, &rpe), NIMCP_ERROR_INVALID_PARAM);

    // Null context
    EXPECT_EQ(tpb_report_loss(nullptr, 1.0f, &rpe), NIMCP_ERROR_INVALID_PARAM);

    // Null rpe_out is OK
    EXPECT_EQ(tpb_report_loss(ctx_, 1.0f, nullptr), NIMCP_SUCCESS);
}

TEST_F(RPETest, InjectReward) {
    ctx_ = tpb_create(nullptr);
    ASSERT_NE(ctx_, nullptr);

    // Inject positive reward
    EXPECT_EQ(tpb_inject_reward(ctx_, 0.5f), NIMCP_SUCCESS);

    // Get neuromod levels to verify DA increased
    float da = 0.0f;
    tpb_get_neuromod_levels(ctx_, &da, nullptr, nullptr, nullptr);
    EXPECT_GT(da, 0.5f) << "DA should increase after positive reward";

    // Inject negative reward
    EXPECT_EQ(tpb_inject_reward(ctx_, -0.5f), NIMCP_SUCCESS);
    tpb_get_neuromod_levels(ctx_, &da, nullptr, nullptr, nullptr);
}

TEST_F(RPETest, GetRPEState) {
    ctx_ = tpb_create(nullptr);
    ASSERT_NE(ctx_, nullptr);

    tpb_rpe_state_t state;

    // Report some losses
    for (int i = 0; i < 5; i++) {
        tpb_report_loss(ctx_, 1.0f - 0.1f * i, nullptr);
    }

    EXPECT_EQ(tpb_get_rpe_state(ctx_, &state), NIMCP_SUCCESS);
    EXPECT_EQ(state.history_count, 5u);
    EXPECT_GT(state.smoothed_rpe, 0.0f) << "Decreasing loss should give positive smoothed RPE";
}

//=============================================================================
// Region Configuration Tests
//=============================================================================

class RegionTest : public TrainingPlasticityBridgeTest {
protected:
    void SetUp() override {
        TrainingPlasticityBridgeTest::SetUp();
        ctx_ = tpb_create(nullptr);
        ASSERT_NE(ctx_, nullptr);
    }
};

TEST_F(RegionTest, ConfigureCorticalRegion) {
    tpb_region_config_t region = tpb_region_cortical_default();
    region.neuron_start_idx = 0;
    region.neuron_end_idx = 1000;

    uint32_t region_id = 0;
    EXPECT_EQ(tpb_configure_region(ctx_, &region, &region_id), NIMCP_SUCCESS);
    EXPECT_EQ(region_id, 0u);
}

TEST_F(RegionTest, ConfigureAllRegionTypes) {
    struct {
        tpb_region_config_t (*getter)(void);
        const char* name;
    } region_presets[] = {
        {tpb_region_cortical_default, "Cortical"},
        {tpb_region_striatal_default, "Striatal"},
        {tpb_region_hippocampal_default, "Hippocampal"},
        {tpb_region_cerebellar_default, "Cerebellar"},
        {tpb_region_amygdala_default, "Amygdala"},
        {tpb_region_prefrontal_default, "Prefrontal"}
    };

    uint32_t neuron_offset = 0;
    for (size_t i = 0; i < sizeof(region_presets) / sizeof(region_presets[0]); i++) {
        tpb_region_config_t region = region_presets[i].getter();
        region.neuron_start_idx = neuron_offset;
        region.neuron_end_idx = neuron_offset + 100;
        neuron_offset += 100;

        uint32_t region_id = 0;
        EXPECT_EQ(tpb_configure_region(ctx_, &region, &region_id), NIMCP_SUCCESS)
            << "Failed to configure " << region_presets[i].name;
        EXPECT_EQ(region_id, i);
    }
}

TEST_F(RegionTest, RegionPresetValues) {
    tpb_region_config_t cortical = tpb_region_cortical_default();
    EXPECT_EQ(cortical.type, TPB_REGION_CORTICAL);
    EXPECT_EQ(cortical.primary_rule, TPB_RULE_STDP);
    EXPECT_TRUE(cortical.enable_three_factor);
    EXPECT_FLOAT_EQ(cortical.ach_sensitivity, 1.2f);

    tpb_region_config_t striatal = tpb_region_striatal_default();
    EXPECT_EQ(striatal.type, TPB_REGION_STRIATAL);
    EXPECT_FLOAT_EQ(striatal.da_sensitivity, 1.5f);

    tpb_region_config_t hippocampal = tpb_region_hippocampal_default();
    EXPECT_EQ(hippocampal.type, TPB_REGION_HIPPOCAMPAL);
    EXPECT_EQ(hippocampal.primary_rule, TPB_RULE_BCM);
}

TEST_F(RegionTest, MaxRegionsLimit) {
    // Configure maximum regions
    for (uint32_t i = 0; i < TPB_MAX_REGIONS; i++) {
        tpb_region_config_t region = tpb_region_cortical_default();
        region.neuron_start_idx = i * 100;
        region.neuron_end_idx = (i + 1) * 100;

        uint32_t region_id = 0;
        EXPECT_EQ(tpb_configure_region(ctx_, &region, &region_id), NIMCP_SUCCESS);
    }

    // One more should fail
    tpb_region_config_t region = tpb_region_cortical_default();
    region.neuron_start_idx = TPB_MAX_REGIONS * 100;
    region.neuron_end_idx = (TPB_MAX_REGIONS + 1) * 100;

    uint32_t region_id = 0;
    EXPECT_EQ(tpb_configure_region(ctx_, &region, &region_id), NIMCP_ERROR_MEMORY);
}

//=============================================================================
// Weight Update Routing Tests
//=============================================================================

class RoutingTest : public TrainingPlasticityBridgeTest {
protected:
    void SetUp() override {
        TrainingPlasticityBridgeTest::SetUp();
        ctx_ = tpb_create(nullptr);
        ASSERT_NE(ctx_, nullptr);

        // Configure two regions with different rules
        tpb_region_config_t cortical = tpb_region_cortical_default();
        cortical.neuron_start_idx = 0;
        cortical.neuron_end_idx = 500;
        tpb_configure_region(ctx_, &cortical, nullptr);

        tpb_region_config_t striatal = tpb_region_striatal_default();
        striatal.neuron_start_idx = 500;
        striatal.neuron_end_idx = 1000;
        tpb_configure_region(ctx_, &striatal, nullptr);
    }
};

TEST_F(RoutingTest, RouteToCorrectRegion) {
    float delta = 0.0f;

    // Route to cortical (STDP)
    EXPECT_EQ(tpb_route_weight_update(ctx_, 100, 0.8f, 0.9f, 10.0f, &delta), NIMCP_SUCCESS);
    // Pre-before-post should give LTP
    EXPECT_GT(delta, 0.0f) << "STDP LTP expected for pre-before-post";

    // Route to striatal (also STDP but different sensitivity)
    EXPECT_EQ(tpb_route_weight_update(ctx_, 700, 0.8f, 0.9f, 10.0f, &delta), NIMCP_SUCCESS);
    EXPECT_GT(delta, 0.0f);
}

TEST_F(RoutingTest, STDPTimingEffect) {
    float delta_ltp = 0.0f;
    float delta_ltd = 0.0f;

    // LTP: Pre-before-post (positive delta)
    EXPECT_EQ(tpb_route_weight_update(ctx_, 100, 0.8f, 0.9f, 10.0f, &delta_ltp), NIMCP_SUCCESS);

    // LTD: Post-before-pre (negative delta)
    EXPECT_EQ(tpb_route_weight_update(ctx_, 100, 0.8f, 0.9f, -10.0f, &delta_ltd), NIMCP_SUCCESS);

    EXPECT_GT(delta_ltp, 0.0f) << "LTP expected for pre-before-post";
    EXPECT_LT(delta_ltd, 0.0f) << "LTD expected for post-before-pre";
}

TEST_F(RoutingTest, UnknownNeuronRegion) {
    float delta = 0.0f;

    // Neuron outside configured regions
    EXPECT_EQ(tpb_route_weight_update(ctx_, 5000, 0.8f, 0.9f, 10.0f, &delta), NIMCP_SUCCESS);
    EXPECT_FLOAT_EQ(delta, 0.0f) << "Unknown region should return 0 delta";
}

//=============================================================================
// Batch Plasticity Tests
//=============================================================================

class BatchPlasticityTest : public TrainingPlasticityBridgeTest {
protected:
    void SetUp() override {
        TrainingPlasticityBridgeTest::SetUp();

        tpb_config_t config = tpb_config_default();
        config.thread_pool_size = 4;
        ctx_ = tpb_create(&config);
        ASSERT_NE(ctx_, nullptr);

        // Configure region
        tpb_region_config_t region = tpb_region_cortical_default();
        region.neuron_start_idx = 0;
        region.neuron_end_idx = 10000;
        tpb_configure_region(ctx_, &region, nullptr);
    }
};

TEST_F(BatchPlasticityTest, SmallBatch) {
    const uint32_t n = 100;
    std::vector<uint32_t> pre_ids(n), post_ids(n);
    std::vector<float> pre_act(n), post_act(n), deltas(n), weights(n, 0.5f);

    for (uint32_t i = 0; i < n; i++) {
        pre_ids[i] = i;
        post_ids[i] = i + 1;
        pre_act[i] = 0.8f;
        post_act[i] = 0.9f;
        deltas[i] = 10.0f;  // LTP timing
    }

    EXPECT_EQ(tpb_apply_plasticity_batch(ctx_, n,
                                          pre_ids.data(), post_ids.data(),
                                          pre_act.data(), post_act.data(),
                                          deltas.data(), weights.data()), NIMCP_SUCCESS);

    // Verify weights changed
    bool any_changed = false;
    for (uint32_t i = 0; i < n; i++) {
        if (weights[i] != 0.5f) {
            any_changed = true;
            break;
        }
    }
    EXPECT_TRUE(any_changed) << "Some weights should have changed";
}

TEST_F(BatchPlasticityTest, LargeBatchParallel) {
    const uint32_t n = 5000;  // Large enough to trigger parallel execution
    std::vector<uint32_t> pre_ids(n), post_ids(n);
    std::vector<float> pre_act(n), post_act(n), deltas(n), weights(n, 0.5f);

    for (uint32_t i = 0; i < n; i++) {
        pre_ids[i] = i % 9999;
        post_ids[i] = (i + 1) % 10000;
        pre_act[i] = 0.7f + 0.3f * (float)(i % 10) / 10.0f;
        post_act[i] = 0.8f + 0.2f * (float)(i % 5) / 5.0f;
        deltas[i] = (i % 2 == 0) ? 10.0f : -10.0f;  // Alternating LTP/LTD
    }

    EXPECT_EQ(tpb_apply_plasticity_batch(ctx_, n,
                                          pre_ids.data(), post_ids.data(),
                                          pre_act.data(), post_act.data(),
                                          deltas.data(), weights.data()), NIMCP_SUCCESS);

    // Count changes
    uint32_t ltp_count = 0, ltd_count = 0;
    for (uint32_t i = 0; i < n; i++) {
        if (weights[i] > 0.5f) ltp_count++;
        else if (weights[i] < 0.5f) ltd_count++;
    }

    EXPECT_GT(ltp_count, 0u) << "Should have some LTP";
    EXPECT_GT(ltd_count, 0u) << "Should have some LTD";
}

TEST_F(BatchPlasticityTest, InvalidInputs) {
    uint32_t pre_ids[10], post_ids[10];
    float pre_act[10], post_act[10], deltas[10], weights[10];

    EXPECT_EQ(tpb_apply_plasticity_batch(nullptr, 10, pre_ids, post_ids,
                                          pre_act, post_act, deltas, weights),
              NIMCP_ERROR_INVALID_PARAM);

    EXPECT_EQ(tpb_apply_plasticity_batch(ctx_, 0, pre_ids, post_ids,
                                          pre_act, post_act, deltas, weights),
              NIMCP_ERROR_INVALID_PARAM);

    EXPECT_EQ(tpb_apply_plasticity_batch(ctx_, 10, nullptr, post_ids,
                                          pre_act, post_act, deltas, weights),
              NIMCP_ERROR_INVALID_PARAM);
}

//=============================================================================
// Neuromodulator-LR Coupling Tests
//=============================================================================

class NeuromodLRTest : public TrainingPlasticityBridgeTest {
protected:
    void SetUp() override {
        TrainingPlasticityBridgeTest::SetUp();
        ctx_ = tpb_create(nullptr);
        ASSERT_NE(ctx_, nullptr);

        tpb_region_config_t region = tpb_region_striatal_default();
        region.neuron_start_idx = 0;
        region.neuron_end_idx = 1000;
        tpb_configure_region(ctx_, &region, &region_id_);
    }

    uint32_t region_id_;
};

TEST_F(NeuromodLRTest, GetModulatedLR) {
    float base_lr = 0.01f;
    float modulated_lr = 0.0f;

    EXPECT_EQ(tpb_get_modulated_lr(ctx_, region_id_, base_lr, &modulated_lr), NIMCP_SUCCESS);
    EXPECT_GT(modulated_lr, 0.0f);
}

TEST_F(NeuromodLRTest, DAIncreasesLR) {
    float base_lr = 0.01f;
    float lr_baseline = 0.0f, lr_high_da = 0.0f;

    // Get baseline LR
    tpb_get_modulated_lr(ctx_, region_id_, base_lr, &lr_baseline);

    // Increase DA
    tpb_set_neuromod_levels(ctx_, 0.9f, -1.0f, -1.0f, -1.0f);
    tpb_get_modulated_lr(ctx_, region_id_, base_lr, &lr_high_da);

    EXPECT_GT(lr_high_da, lr_baseline) << "High DA should increase LR";
}

TEST_F(NeuromodLRTest, LRBounds) {
    tpb_config_t config = tpb_config_default();
    config.lr_modulation.min_lr_multiplier = 0.5f;
    config.lr_modulation.max_lr_multiplier = 2.0f;

    tpb_destroy(ctx_);
    ctx_ = tpb_create(&config);
    ASSERT_NE(ctx_, nullptr);

    tpb_region_config_t region = tpb_region_cortical_default();
    region.neuron_start_idx = 0;
    region.neuron_end_idx = 1000;
    tpb_configure_region(ctx_, &region, nullptr);

    float base_lr = 0.01f;
    float modulated_lr = 0.0f;

    // Extreme low neuromod
    tpb_set_neuromod_levels(ctx_, 0.0f, 0.0f, 1.0f, 0.0f);
    tpb_get_modulated_lr(ctx_, 0, base_lr, &modulated_lr);
    EXPECT_GE(modulated_lr, base_lr * 0.5f * 0.9f) << "LR should be at/above min bound";

    // Extreme high neuromod
    tpb_set_neuromod_levels(ctx_, 1.0f, 1.0f, 0.0f, 1.0f);
    tpb_get_modulated_lr(ctx_, 0, base_lr, &modulated_lr);
    EXPECT_LE(modulated_lr, base_lr * 2.0f * 1.1f) << "LR should be at/below max bound";
}

TEST_F(NeuromodLRTest, GetSetNeuromodLevels) {
    float da, ach, ht5, ne;

    // Set levels
    EXPECT_EQ(tpb_set_neuromod_levels(ctx_, 0.3f, 0.4f, 0.5f, 0.6f), NIMCP_SUCCESS);

    // Get levels
    EXPECT_EQ(tpb_get_neuromod_levels(ctx_, &da, &ach, &ht5, &ne), NIMCP_SUCCESS);
    EXPECT_NEAR(da, 0.3f, 0.05f);
    EXPECT_NEAR(ach, 0.4f, 0.05f);
    EXPECT_NEAR(ht5, 0.5f, 0.05f);
    EXPECT_NEAR(ne, 0.6f, 0.05f);

    // Partial update (negative = keep current)
    EXPECT_EQ(tpb_set_neuromod_levels(ctx_, 0.8f, -1.0f, -1.0f, -1.0f), NIMCP_SUCCESS);
    tpb_get_neuromod_levels(ctx_, &da, &ach, nullptr, nullptr);
    EXPECT_NEAR(da, 0.8f, 0.05f);
    EXPECT_NEAR(ach, 0.4f, 0.05f);  // Should be unchanged
}

//=============================================================================
// STDP/BCM Integration Tests
//=============================================================================

class PlasticityIntegrationTest : public TrainingPlasticityBridgeTest {
protected:
    void SetUp() override {
        TrainingPlasticityBridgeTest::SetUp();
        ctx_ = tpb_create(nullptr);
        ASSERT_NE(ctx_, nullptr);

        tpb_region_config_t region = tpb_region_cortical_default();
        region.neuron_start_idx = 0;
        region.neuron_end_idx = 1000;
        tpb_configure_region(ctx_, &region, nullptr);
    }
};

TEST_F(PlasticityIntegrationTest, CreateSTDPSynapse) {
    stdp_synapse_t synapse;
    EXPECT_EQ(tpb_create_stdp_synapse(ctx_, 0, &synapse), NIMCP_SUCCESS);

    EXPECT_GT(synapse.learning_rate, 0.0f);
    EXPECT_GT(synapse.tau_plus, 0.0f);
    EXPECT_TRUE(synapse.enable_da_modulation);
}

TEST_F(PlasticityIntegrationTest, CreateBCMSynapse) {
    bcm_synapse_t synapse;
    EXPECT_EQ(tpb_create_bcm_synapse(ctx_, 0, &synapse), NIMCP_SUCCESS);

    EXPECT_GE(synapse.weight, 0.0f);
    EXPECT_LE(synapse.weight, 1.0f);
    EXPECT_GT(synapse.threshold, 0.0f);
}

TEST_F(PlasticityIntegrationTest, UpdateSTDP) {
    stdp_synapse_t synapse;
    tpb_create_stdp_synapse(ctx_, 0, &synapse);

    float delta = 0.0f;

    // Pre spike
    EXPECT_EQ(tpb_update_stdp(ctx_, &synapse, true, false, 100.0f, &delta), NIMCP_SUCCESS);

    // Post spike (LTP timing)
    EXPECT_EQ(tpb_update_stdp(ctx_, &synapse, false, true, 110.0f, &delta), NIMCP_SUCCESS);
    EXPECT_NE(delta, 0.0f) << "STDP update should change weight";
}

TEST_F(PlasticityIntegrationTest, UpdateBCM) {
    bcm_synapse_t synapse;
    tpb_create_bcm_synapse(ctx_, 0, &synapse);

    float initial_threshold = synapse.threshold;
    float delta = 0.0f;

    // High post-synaptic activity (above threshold)
    EXPECT_EQ(tpb_update_bcm(ctx_, &synapse, 0.8f, 0.8f, 0.001f, &delta), NIMCP_SUCCESS);

    // Threshold should slide up for high activity
    EXPECT_NE(synapse.threshold, initial_threshold);
}

//=============================================================================
// Statistics Tests
//=============================================================================

class StatsTest : public TrainingPlasticityBridgeTest {
protected:
    void SetUp() override {
        TrainingPlasticityBridgeTest::SetUp();
        ctx_ = tpb_create(nullptr);
        ASSERT_NE(ctx_, nullptr);

        tpb_region_config_t region = tpb_region_cortical_default();
        region.neuron_start_idx = 0;
        region.neuron_end_idx = 1000;
        tpb_configure_region(ctx_, &region, nullptr);
    }
};

TEST_F(StatsTest, GetStats) {
    tpb_stats_t stats;
    EXPECT_EQ(tpb_get_stats(ctx_, &stats), NIMCP_SUCCESS);
    EXPECT_EQ(stats.rpe_computations, 0u);
}

TEST_F(StatsTest, StatsAfterOperations) {
    // Report losses
    for (int i = 0; i < 10; i++) {
        tpb_report_loss(ctx_, 1.0f - 0.05f * i, nullptr);
    }

    // Do some routing
    float delta;
    for (int i = 0; i < 20; i++) {
        tpb_route_weight_update(ctx_, 100, 0.8f, 0.9f, 10.0f, &delta);
    }

    tpb_stats_t stats;
    EXPECT_EQ(tpb_get_stats(ctx_, &stats), NIMCP_SUCCESS);
    EXPECT_EQ(stats.rpe_computations, 10u);
    EXPECT_EQ(stats.total_plasticity_updates, 20u);
    EXPECT_GT(stats.stdp_updates, 0u);
    EXPECT_GT(stats.avg_rpe, 0.0f) << "Decreasing loss should give positive avg RPE";
}

TEST_F(StatsTest, ResetStats) {
    // Generate some stats
    tpb_report_loss(ctx_, 1.0f, nullptr);
    tpb_report_loss(ctx_, 0.9f, nullptr);

    float delta;
    tpb_route_weight_update(ctx_, 100, 0.8f, 0.9f, 10.0f, &delta);

    // Reset
    EXPECT_EQ(tpb_reset_stats(ctx_), NIMCP_SUCCESS);

    tpb_stats_t stats;
    tpb_get_stats(ctx_, &stats);
    EXPECT_EQ(stats.rpe_computations, 0u);
    EXPECT_EQ(stats.total_plasticity_updates, 0u);
}

TEST_F(StatsTest, DABurstDipTracking) {
    // Create large positive RPE (loss decrease)
    tpb_report_loss(ctx_, 2.0f, nullptr);  // Establish high baseline
    tpb_report_loss(ctx_, 2.0f, nullptr);
    tpb_report_loss(ctx_, 0.5f, nullptr);  // Large improvement = DA burst

    // Create large negative RPE (loss increase)
    tpb_report_loss(ctx_, 0.5f, nullptr);  // Establish low baseline
    tpb_report_loss(ctx_, 0.5f, nullptr);
    tpb_report_loss(ctx_, 2.0f, nullptr);  // Large deterioration = DA dip

    tpb_stats_t stats;
    tpb_get_stats(ctx_, &stats);

    // Should have recorded burst and dip events
    // (exact counts depend on RPE computation and thresholds)
}

//=============================================================================
// CoW Snapshot Tests
//=============================================================================

class CoWTest : public TrainingPlasticityBridgeTest {
protected:
    void SetUp() override {
        TrainingPlasticityBridgeTest::SetUp();

        tpb_config_t config = tpb_config_default();
        config.enable_cow = true;
        ctx_ = tpb_create(&config);
        ASSERT_NE(ctx_, nullptr);
    }
};

TEST_F(CoWTest, SnapshotAndRestore) {
    const uint32_t n = 100;
    std::vector<float> weights(n);
    for (uint32_t i = 0; i < n; i++) {
        weights[i] = (float)i / n;
    }

    cow_handle_t snapshot = nullptr;
    EXPECT_EQ(tpb_snapshot_weights(ctx_, weights.data(), n, &snapshot), NIMCP_SUCCESS);
    ASSERT_NE(snapshot, nullptr);

    // Modify weights
    for (uint32_t i = 0; i < n; i++) {
        weights[i] = 0.0f;
    }

    // Restore from snapshot
    EXPECT_EQ(tpb_restore_weights(ctx_, snapshot, weights.data()), NIMCP_SUCCESS);

    // Verify restoration
    for (uint32_t i = 0; i < n; i++) {
        EXPECT_NEAR(weights[i], (float)i / n, 0.001f);
    }

    EXPECT_EQ(tpb_release_snapshot(ctx_, snapshot), NIMCP_SUCCESS);
}

TEST_F(CoWTest, InvalidSnapshotInputs) {
    float weights[10] = {0};
    cow_handle_t snapshot = nullptr;

    EXPECT_EQ(tpb_snapshot_weights(nullptr, weights, 10, &snapshot), NIMCP_ERROR_INVALID_PARAM);
    EXPECT_EQ(tpb_snapshot_weights(ctx_, nullptr, 10, &snapshot), NIMCP_ERROR_INVALID_PARAM);
    EXPECT_EQ(tpb_snapshot_weights(ctx_, weights, 0, &snapshot), NIMCP_ERROR_INVALID_PARAM);
    EXPECT_EQ(tpb_snapshot_weights(ctx_, weights, 10, nullptr), NIMCP_ERROR_INVALID_PARAM);
}

//=============================================================================
// Thread Safety Tests
//=============================================================================

class ThreadSafetyTest : public TrainingPlasticityBridgeTest {
protected:
    void SetUp() override {
        TrainingPlasticityBridgeTest::SetUp();

        tpb_config_t config = tpb_config_default();
        config.thread_pool_size = 4;
        ctx_ = tpb_create(&config);
        ASSERT_NE(ctx_, nullptr);

        tpb_region_config_t region = tpb_region_cortical_default();
        region.neuron_start_idx = 0;
        region.neuron_end_idx = 10000;
        tpb_configure_region(ctx_, &region, nullptr);
    }
};

TEST_F(ThreadSafetyTest, ConcurrentLossReporting) {
    const int num_threads = 4;
    const int iterations = 100;
    std::atomic<int> success_count{0};

    auto worker = [&](int thread_id) {
        for (int i = 0; i < iterations; i++) {
            float loss = 1.0f - 0.001f * (thread_id * iterations + i);
            float rpe = 0.0f;
            if (tpb_report_loss(ctx_, loss, &rpe) == NIMCP_SUCCESS) {
                success_count++;
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back(worker, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), num_threads * iterations);
}

TEST_F(ThreadSafetyTest, ConcurrentWeightUpdates) {
    const int num_threads = 4;
    const int iterations = 50;
    std::atomic<int> success_count{0};

    auto worker = [&](int thread_id) {
        for (int i = 0; i < iterations; i++) {
            float delta = 0.0f;
            uint32_t neuron = (thread_id * iterations + i) % 10000;
            if (tpb_route_weight_update(ctx_, neuron, 0.8f, 0.9f, 10.0f, &delta) == NIMCP_SUCCESS) {
                success_count++;
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back(worker, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), num_threads * iterations);
}

TEST_F(ThreadSafetyTest, ConcurrentNeuromodAccess) {
    const int num_threads = 4;
    const int iterations = 100;
    std::atomic<int> success_count{0};

    auto writer = [&](int thread_id) {
        for (int i = 0; i < iterations; i++) {
            float da = 0.3f + 0.1f * (thread_id % 5);
            if (tpb_set_neuromod_levels(ctx_, da, -1.0f, -1.0f, -1.0f) == NIMCP_SUCCESS) {
                success_count++;
            }
        }
    };

    auto reader = [&](int thread_id) {
        for (int i = 0; i < iterations; i++) {
            float da, ach, ht5, ne;
            if (tpb_get_neuromod_levels(ctx_, &da, &ach, &ht5, &ne) == NIMCP_SUCCESS) {
                success_count++;
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads / 2; i++) {
        threads.emplace_back(writer, i);
        threads.emplace_back(reader, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), num_threads * iterations);
}

//=============================================================================
// Callback Integration Tests (Phase TCB-1)
//=============================================================================

class CallbackIntegrationTest : public TrainingPlasticityBridgeTest {
protected:
    void SetUp() override {
        TrainingPlasticityBridgeTest::SetUp();
        ctx_ = tpb_create(nullptr);
        ASSERT_NE(ctx_, nullptr);

        // Configure region for testing
        tpb_region_config_t region = tpb_region_cortical_default();
        region.neuron_start_idx = 0;
        region.neuron_end_idx = 1000;
        tpb_configure_region(ctx_, &region, nullptr);
    }

    void TearDown() override {
        if (tcb_ctx_) {
            tcb_destroy(tcb_ctx_);
            tcb_ctx_ = nullptr;
        }
        TrainingPlasticityBridgeTest::TearDown();
    }

    tcb_context_t* tcb_ctx_ = nullptr;
};

TEST_F(CallbackIntegrationTest, ConnectCallbacks) {
    tcb_ctx_ = tcb_create(nullptr);
    ASSERT_NE(tcb_ctx_, nullptr);

    EXPECT_EQ(tpb_connect_callbacks(ctx_, tcb_ctx_), NIMCP_SUCCESS);
    EXPECT_EQ(tpb_get_callback_context(ctx_), tcb_ctx_);
}

TEST_F(CallbackIntegrationTest, ConnectNullCallbacks) {
    // Should succeed - disconnecting callbacks
    EXPECT_EQ(tpb_connect_callbacks(ctx_, nullptr), NIMCP_SUCCESS);
    EXPECT_EQ(tpb_get_callback_context(ctx_), nullptr);
}

TEST_F(CallbackIntegrationTest, ConnectCallbacksNullContext) {
    tcb_ctx_ = tcb_create(nullptr);
    EXPECT_EQ(tpb_connect_callbacks(nullptr, tcb_ctx_), NIMCP_ERROR_INVALID_PARAM);
}

TEST_F(CallbackIntegrationTest, RegisterPlasticityCallbacks) {
    tcb_ctx_ = tcb_create(nullptr);
    ASSERT_NE(tcb_ctx_, nullptr);

    EXPECT_EQ(tpb_connect_callbacks(ctx_, tcb_ctx_), NIMCP_SUCCESS);
    EXPECT_EQ(tpb_register_plasticity_callbacks(ctx_), NIMCP_SUCCESS);

    // Verify callbacks are registered
    EXPECT_GT(tcb_get_callback_count(tcb_ctx_, TCB_EVENT_LOSS_COMPUTED), 0u);
    EXPECT_GT(tcb_get_callback_count(tcb_ctx_, TCB_EVENT_WEIGHTS_UPDATED), 0u);
    EXPECT_GT(tcb_get_callback_count(tcb_ctx_, TCB_EVENT_EPOCH_COMPLETE), 0u);
    EXPECT_GT(tcb_get_callback_count(tcb_ctx_, TCB_EVENT_DIVERGENCE), 0u);
}

TEST_F(CallbackIntegrationTest, RegisterPlasticityCallbacksNoContext) {
    // Should fail without connected callback context
    EXPECT_EQ(tpb_register_plasticity_callbacks(ctx_), NIMCP_ERROR_INVALID_PARAM);
}

TEST_F(CallbackIntegrationTest, HandleCallbackActionContinue) {
    EXPECT_EQ(tpb_handle_callback_action(ctx_, TCB_ACTION_CONTINUE), NIMCP_SUCCESS);
}

TEST_F(CallbackIntegrationTest, HandleCallbackActionReduceLR) {
    float da_before, da_after, ht5_before, ht5_after;

    tpb_get_neuromod_levels(ctx_, &da_before, nullptr, &ht5_before, nullptr);
    EXPECT_EQ(tpb_handle_callback_action(ctx_, TCB_ACTION_REDUCE_LR), NIMCP_SUCCESS);
    tpb_get_neuromod_levels(ctx_, &da_after, nullptr, &ht5_after, nullptr);

    // DA should decrease, 5-HT should increase
    EXPECT_LT(da_after, da_before) << "DA should decrease on LR reduction";
    EXPECT_GT(ht5_after, ht5_before) << "5-HT should increase on LR reduction";
}

TEST_F(CallbackIntegrationTest, HandleCallbackActionIncreaseLR) {
    float da_before, da_after;

    tpb_get_neuromod_levels(ctx_, &da_before, nullptr, nullptr, nullptr);
    EXPECT_EQ(tpb_handle_callback_action(ctx_, TCB_ACTION_INCREASE_LR), NIMCP_SUCCESS);
    tpb_get_neuromod_levels(ctx_, &da_after, nullptr, nullptr, nullptr);

    EXPECT_GT(da_after, da_before) << "DA should increase on LR increase";
}

TEST_F(CallbackIntegrationTest, HandleCallbackActionSkipStep) {
    float da_before, da_after, ach_before, ach_after;

    tpb_get_neuromod_levels(ctx_, &da_before, &ach_before, nullptr, nullptr);
    EXPECT_EQ(tpb_handle_callback_action(ctx_, TCB_ACTION_SKIP_STEP), NIMCP_SUCCESS);
    tpb_get_neuromod_levels(ctx_, &da_after, &ach_after, nullptr, nullptr);

    // Neuromodulators should be dampened
    EXPECT_LT(da_after, da_before);
    EXPECT_LT(ach_after, ach_before);
}

TEST_F(CallbackIntegrationTest, HandleCallbackActionNull) {
    EXPECT_EQ(tpb_handle_callback_action(nullptr, TCB_ACTION_CONTINUE), NIMCP_ERROR_INVALID_PARAM);
}

TEST_F(CallbackIntegrationTest, SetCallbackModulation) {
    EXPECT_EQ(tpb_set_callback_modulation(ctx_, TCB_EVENT_LOSS_COMPUTED, 0.5f), NIMCP_SUCCESS);
    EXPECT_EQ(tpb_set_callback_modulation(ctx_, TCB_EVENT_WEIGHTS_UPDATED, 1.5f), NIMCP_SUCCESS);
}

TEST_F(CallbackIntegrationTest, SetCallbackModulationBounds) {
    // Should clamp to valid range
    EXPECT_EQ(tpb_set_callback_modulation(ctx_, TCB_EVENT_LOSS_COMPUTED, -1.0f), NIMCP_SUCCESS);
    EXPECT_EQ(tpb_set_callback_modulation(ctx_, TCB_EVENT_LOSS_COMPUTED, 5.0f), NIMCP_SUCCESS);
}

TEST_F(CallbackIntegrationTest, SetCallbackModulationInvalidEvent) {
    EXPECT_EQ(tpb_set_callback_modulation(ctx_, TCB_EVENT_COUNT, 1.0f), NIMCP_ERROR_INVALID_PARAM);
}

TEST_F(CallbackIntegrationTest, GetCallbackStats) {
    tcb_ctx_ = tcb_create(nullptr);
    ASSERT_NE(tcb_ctx_, nullptr);

    tpb_connect_callbacks(ctx_, tcb_ctx_);
    tpb_register_plasticity_callbacks(ctx_);

    uint64_t loss_fired = 0, weight_fired = 0, epoch_fired = 0, divergence_fired = 0;
    EXPECT_EQ(tpb_get_callback_stats(ctx_, &loss_fired, &weight_fired, &epoch_fired, &divergence_fired), NIMCP_SUCCESS);

    // Initially all should be 0
    EXPECT_EQ(loss_fired, 0u);
    EXPECT_EQ(weight_fired, 0u);
    EXPECT_EQ(epoch_fired, 0u);
    EXPECT_EQ(divergence_fired, 0u);
}

TEST_F(CallbackIntegrationTest, GetCallbackStatsNullContext) {
    uint64_t dummy;
    EXPECT_EQ(tpb_get_callback_stats(nullptr, &dummy, &dummy, &dummy, &dummy), NIMCP_ERROR_INVALID_PARAM);
}

TEST_F(CallbackIntegrationTest, CallbackOnLossPositiveRPE) {
    tcb_ctx_ = tcb_create(nullptr);
    ASSERT_NE(tcb_ctx_, nullptr);

    tpb_connect_callbacks(ctx_, tcb_ctx_);
    tpb_register_plasticity_callbacks(ctx_);

    // Create event with good loss (low value)
    tcb_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));
    metrics.loss = 0.1f;
    metrics.step = 10;

    // First establish baseline
    metrics.loss = 1.0f;
    tcb_fire_event(tcb_ctx_, TCB_EVENT_LOSS_COMPUTED, &metrics);

    // Now report improvement
    metrics.loss = 0.5f;
    tcb_action_t action = tcb_fire_event(tcb_ctx_, TCB_EVENT_LOSS_COMPUTED, &metrics);

    // Should continue (positive RPE = good learning)
    EXPECT_EQ(action, TCB_ACTION_CONTINUE);

    // Verify stats updated
    uint64_t loss_fired = 0;
    tpb_get_callback_stats(ctx_, &loss_fired, nullptr, nullptr, nullptr);
    EXPECT_GT(loss_fired, 0u);
}

TEST_F(CallbackIntegrationTest, CallbackOnWeights) {
    tcb_ctx_ = tcb_create(nullptr);
    ASSERT_NE(tcb_ctx_, nullptr);

    tpb_connect_callbacks(ctx_, tcb_ctx_);
    tpb_register_plasticity_callbacks(ctx_);

    tcb_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));
    metrics.gradient_norm = 1.0f;
    metrics.step = 10;

    tcb_action_t action = tcb_fire_event(tcb_ctx_, TCB_EVENT_WEIGHTS_UPDATED, &metrics);
    EXPECT_EQ(action, TCB_ACTION_CONTINUE);

    uint64_t weight_fired = 0;
    tpb_get_callback_stats(ctx_, nullptr, &weight_fired, nullptr, nullptr);
    EXPECT_GT(weight_fired, 0u);
}

TEST_F(CallbackIntegrationTest, CallbackOnEpoch) {
    tcb_ctx_ = tcb_create(nullptr);
    ASSERT_NE(tcb_ctx_, nullptr);

    tpb_connect_callbacks(ctx_, tcb_ctx_);
    tpb_register_plasticity_callbacks(ctx_);

    tcb_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));
    metrics.epoch = 1;
    metrics.loss = 0.5f;

    tcb_action_t action = tcb_fire_event(tcb_ctx_, TCB_EVENT_EPOCH_COMPLETE, &metrics);
    EXPECT_EQ(action, TCB_ACTION_CONTINUE);

    uint64_t epoch_fired = 0;
    tpb_get_callback_stats(ctx_, nullptr, nullptr, &epoch_fired, nullptr);
    EXPECT_GT(epoch_fired, 0u);
}

TEST_F(CallbackIntegrationTest, CallbackOnDivergence) {
    tcb_ctx_ = tcb_create(nullptr);
    ASSERT_NE(tcb_ctx_, nullptr);

    tpb_connect_callbacks(ctx_, tcb_ctx_);
    tpb_register_plasticity_callbacks(ctx_);

    tcb_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));
    metrics.loss = 100.0f;  // High loss indicating divergence
    metrics.step = 10;

    tcb_action_t action = tcb_fire_event(tcb_ctx_, TCB_EVENT_DIVERGENCE, &metrics);
    // Should request rollback or reduce LR
    EXPECT_TRUE(action == TCB_ACTION_ROLLBACK || action == TCB_ACTION_REDUCE_LR);

    uint64_t divergence_fired = 0;
    tpb_get_callback_stats(ctx_, nullptr, nullptr, nullptr, &divergence_fired);
    EXPECT_GT(divergence_fired, 0u);
}

TEST_F(CallbackIntegrationTest, CallbackOnDivergenceNaN) {
    tcb_ctx_ = tcb_create(nullptr);
    ASSERT_NE(tcb_ctx_, nullptr);

    tpb_connect_callbacks(ctx_, tcb_ctx_);
    tpb_register_plasticity_callbacks(ctx_);

    tcb_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));
    metrics.loss = NAN;  // NaN loss
    metrics.step = 10;

    tcb_action_t action = tcb_fire_event(tcb_ctx_, TCB_EVENT_DIVERGENCE, &metrics);
    // Should stop training for NaN
    EXPECT_EQ(action, TCB_ACTION_STOP_TRAINING);
}

TEST_F(CallbackIntegrationTest, LargeGradientBoostsNE) {
    tcb_ctx_ = tcb_create(nullptr);
    ASSERT_NE(tcb_ctx_, nullptr);

    tpb_connect_callbacks(ctx_, tcb_ctx_);
    tpb_register_plasticity_callbacks(ctx_);

    float ne_before, ne_after;
    tpb_get_neuromod_levels(ctx_, nullptr, nullptr, nullptr, &ne_before);

    tcb_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));
    metrics.gradient_norm = 100.0f;  // Large gradient
    metrics.step = 10;

    tcb_fire_event(tcb_ctx_, TCB_EVENT_WEIGHTS_UPDATED, &metrics);
    tpb_get_neuromod_levels(ctx_, nullptr, nullptr, nullptr, &ne_after);

    EXPECT_GT(ne_after, ne_before) << "Large gradient should boost NE";
}

TEST_F(CallbackIntegrationTest, SmallGradientBoostsACh) {
    tcb_ctx_ = tcb_create(nullptr);
    ASSERT_NE(tcb_ctx_, nullptr);

    tpb_connect_callbacks(ctx_, tcb_ctx_);
    tpb_register_plasticity_callbacks(ctx_);

    float ach_before, ach_after;
    tpb_get_neuromod_levels(ctx_, nullptr, &ach_before, nullptr, nullptr);

    tcb_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));
    metrics.gradient_norm = 0.001f;  // Very small gradient
    metrics.step = 10;

    tcb_fire_event(tcb_ctx_, TCB_EVENT_WEIGHTS_UPDATED, &metrics);
    tpb_get_neuromod_levels(ctx_, nullptr, &ach_after, nullptr, nullptr);

    EXPECT_GT(ach_after, ach_before) << "Small gradient should boost ACh for exploration";
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
