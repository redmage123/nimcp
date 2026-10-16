/**
 * @file test_dendrite_gpu.cpp
 * @brief Unit tests for GPU-accelerated dendrite module
 *
 * Tests dendrite_gpu_* APIs for tensor-based GPU dendritic computation
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>

// Include GPU headers before extern "C"
#include "gpu/dendrite/nimcp_dendrite_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"

// Headers have their own extern "C" guards
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Constants
//=============================================================================

static const uint32_t TEST_NUM_DENDRITES = 64;
static const uint32_t TEST_NUM_SEGMENTS = 32;
static const uint32_t TEST_NUM_SPINES = 128;
static const uint32_t SMALL_NETWORK = 16;
static const uint32_t LARGE_NETWORK = 1024;

//=============================================================================
// Test Fixture
//=============================================================================

class DendriteGPUTest : public ::testing::Test {
protected:
    nimcp_gpu_context_t* gpu_ctx = nullptr;
    dendrite_gpu_context_t* dend_ctx = nullptr;
    bool gpu_available = false;

    void SetUp() override {
        gpu_ctx = nimcp_gpu_context_create(0);
        gpu_available = (gpu_ctx != nullptr);
    }

    void TearDown() override {
        if (dend_ctx) {
            dendrite_gpu_destroy(dend_ctx);
            dend_ctx = nullptr;
        }
        if (gpu_ctx) {
            nimcp_gpu_context_destroy(gpu_ctx);
            gpu_ctx = nullptr;
        }
    }

    void CreateDendriteContext() {
        if (!gpu_available) {
            GTEST_SKIP() << "GPU not available";
        }

        dendrite_gpu_config_t config = dendrite_gpu_default_config();
        config.max_dendrites = TEST_NUM_DENDRITES * 2;
        config.max_segments_per_dendrite = TEST_NUM_SEGMENTS;
        config.max_spines_per_dendrite = TEST_NUM_SPINES;
        dend_ctx = dendrite_gpu_create(gpu_ctx, &config);
        ASSERT_NE(dend_ctx, nullptr) << "Failed to create dendrite context";
    }

    void UploadTestData() {
        // Create test segments
        std::vector<dendrite_gpu_segment_t> segments(TEST_NUM_DENDRITES * TEST_NUM_SEGMENTS);
        for (uint32_t d = 0; d < TEST_NUM_DENDRITES; d++) {
            for (uint32_t s = 0; s < TEST_NUM_SEGMENTS; s++) {
                auto& seg = segments[d * TEST_NUM_SEGMENTS + s];
                seg.segment_id = s;
                seg.dendrite_id = d;
                seg.parent_segment = (s > 0) ? s - 1 : 0;
                seg.length = 10.0f;
                seg.diameter = 1.0f;
                seg.path_distance = s * 10.0f;
                seg.rm = 10000.0f;
                seg.cm = 1.0f;
                seg.ra = 100.0f;
                seg.voltage = -70.0f;
                seg.calcium = 0.0f;
                seg.has_active_properties = 0;
            }
        }
        dendrite_gpu_upload_segments(dend_ctx, segments.data(),
                                     TEST_NUM_DENDRITES, TEST_NUM_SEGMENTS);

        // Create test spines
        std::vector<dendrite_gpu_spine_t> spines(TEST_NUM_DENDRITES * TEST_NUM_SPINES);
        for (uint32_t d = 0; d < TEST_NUM_DENDRITES; d++) {
            for (uint32_t sp = 0; sp < TEST_NUM_SPINES; sp++) {
                auto& spine = spines[d * TEST_NUM_SPINES + sp];
                spine.spine_id = sp;
                spine.dendrite_id = d;
                spine.segment_id = sp % TEST_NUM_SEGMENTS;
                spine.neck_resistance = 500.0f;
                spine.head_capacitance = 0.1f;
                spine.voltage = -70.0f;
                spine.calcium = 0.0f;
                spine.synaptic_weight = 0.5f;
                spine.ampa_conductance = 0.0f;
                spine.nmda_conductance = 0.0f;
                spine.pre_trace = 0.0f;
                spine.post_trace = 0.0f;
                spine.last_pre_spike = 0;
                spine.last_post_spike = 0;
            }
        }
        dendrite_gpu_upload_spines(dend_ctx, spines.data(),
                                   TEST_NUM_DENDRITES, TEST_NUM_SPINES);
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(DendriteGPUTest, DefaultConfig_HasValidValues) {
    dendrite_gpu_config_t config = dendrite_gpu_default_config();

    EXPECT_GT(config.max_dendrites, 0);
    EXPECT_GT(config.max_segments_per_dendrite, 0);
    EXPECT_GT(config.max_spines_per_dendrite, 0);
    EXPECT_GT(config.default_rm, 0.0f);
    EXPECT_GT(config.default_cm, 0.0f);
    EXPECT_GT(config.default_ra, 0.0f);
    EXPECT_GT(config.calcium_decay_tau_ms, 0.0f);
    EXPECT_GT(config.stdp_tau_plus_ms, 0.0f);
    EXPECT_GT(config.stdp_tau_minus_ms, 0.0f);
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(DendriteGPUTest, Create_NullContext_ReturnsNull) {
    dendrite_gpu_config_t config = dendrite_gpu_default_config();
    dendrite_gpu_context_t* ctx = dendrite_gpu_create(nullptr, &config);
    EXPECT_EQ(ctx, nullptr);
}

TEST_F(DendriteGPUTest, Create_NullConfig_UsesDefaults) {
    if (!gpu_available) GTEST_SKIP() << "GPU not available";

    dendrite_gpu_context_t* ctx = dendrite_gpu_create(gpu_ctx, nullptr);
    EXPECT_NE(ctx, nullptr);
    dendrite_gpu_destroy(ctx);
}

TEST_F(DendriteGPUTest, Create_ValidConfig_Succeeds) {
    if (!gpu_available) GTEST_SKIP() << "GPU not available";

    dendrite_gpu_config_t config = dendrite_gpu_default_config();
    config.max_dendrites = 256;
    config.max_segments_per_dendrite = 64;

    dendrite_gpu_context_t* ctx = dendrite_gpu_create(gpu_ctx, &config);
    EXPECT_NE(ctx, nullptr);
    dendrite_gpu_destroy(ctx);
}

TEST_F(DendriteGPUTest, Destroy_Null_DoesNotCrash) {
    dendrite_gpu_destroy(nullptr);
    SUCCEED();
}

TEST_F(DendriteGPUTest, Synchronize_ValidContext_Succeeds) {
    CreateDendriteContext();
    EXPECT_TRUE(dendrite_gpu_synchronize(dend_ctx));
}

TEST_F(DendriteGPUTest, Synchronize_NullContext_ReturnsFalse) {
    EXPECT_FALSE(dendrite_gpu_synchronize(nullptr));
}

//=============================================================================
// Data Upload Tests
//=============================================================================

TEST_F(DendriteGPUTest, UploadSegments_ValidData_Succeeds) {
    CreateDendriteContext();

    std::vector<dendrite_gpu_segment_t> segments(TEST_NUM_DENDRITES * TEST_NUM_SEGMENTS);
    for (auto& seg : segments) {
        seg.voltage = -70.0f;
        seg.length = 10.0f;
        seg.diameter = 1.0f;
    }

    EXPECT_TRUE(dendrite_gpu_upload_segments(
        dend_ctx, segments.data(), TEST_NUM_DENDRITES, TEST_NUM_SEGMENTS));
}

TEST_F(DendriteGPUTest, UploadSegments_NullData_ReturnsFalse) {
    CreateDendriteContext();
    EXPECT_FALSE(dendrite_gpu_upload_segments(
        dend_ctx, nullptr, TEST_NUM_DENDRITES, TEST_NUM_SEGMENTS));
}

TEST_F(DendriteGPUTest, UploadSpines_ValidData_Succeeds) {
    CreateDendriteContext();

    std::vector<dendrite_gpu_spine_t> spines(TEST_NUM_DENDRITES * TEST_NUM_SPINES);
    for (auto& spine : spines) {
        spine.voltage = -70.0f;
        spine.calcium = 0.0f;
        spine.synaptic_weight = 0.5f;
    }

    EXPECT_TRUE(dendrite_gpu_upload_spines(
        dend_ctx, spines.data(), TEST_NUM_DENDRITES, TEST_NUM_SPINES));
}

TEST_F(DendriteGPUTest, UploadCableParams_ValidData_Succeeds) {
    CreateDendriteContext();

    std::vector<float> rm(TEST_NUM_DENDRITES, 10000.0f);
    std::vector<float> cm(TEST_NUM_DENDRITES, 1.0f);
    std::vector<float> ra(TEST_NUM_DENDRITES, 100.0f);

    EXPECT_TRUE(dendrite_gpu_upload_cable_params(
        dend_ctx, rm.data(), cm.data(), ra.data(), TEST_NUM_DENDRITES));
}

//=============================================================================
// Download Tests
//=============================================================================

TEST_F(DendriteGPUTest, DownloadVoltages_ValidContext_Succeeds) {
    CreateDendriteContext();
    UploadTestData();

    std::vector<float> voltages(TEST_NUM_DENDRITES * TEST_NUM_SEGMENTS);
    EXPECT_TRUE(dendrite_gpu_download_voltages(dend_ctx, voltages.data()));
}

TEST_F(DendriteGPUTest, DownloadCalcium_ValidContext_Succeeds) {
    CreateDendriteContext();
    UploadTestData();

    std::vector<float> calcium(TEST_NUM_DENDRITES * TEST_NUM_SPINES);
    EXPECT_TRUE(dendrite_gpu_download_calcium(dend_ctx, calcium.data()));
}

TEST_F(DendriteGPUTest, DownloadWeights_ValidContext_Succeeds) {
    CreateDendriteContext();
    UploadTestData();

    std::vector<float> weights(TEST_NUM_DENDRITES * TEST_NUM_SPINES);
    EXPECT_TRUE(dendrite_gpu_download_weights(dend_ctx, weights.data()));
}

TEST_F(DendriteGPUTest, DownloadNMDAStates_ValidContext_Succeeds) {
    CreateDendriteContext();
    UploadTestData();

    std::vector<float> nmda(TEST_NUM_DENDRITES * TEST_NUM_SEGMENTS);
    EXPECT_TRUE(dendrite_gpu_download_nmda_states(dend_ctx, nmda.data()));
}

//=============================================================================
// Core Computation Tests
//=============================================================================

TEST_F(DendriteGPUTest, Integrate_ValidContext_Succeeds) {
    CreateDendriteContext();
    UploadTestData();

    EXPECT_TRUE(dendrite_gpu_integrate(dend_ctx, 0.1f));
}

TEST_F(DendriteGPUTest, Integrate_NullContext_ReturnsFalse) {
    EXPECT_FALSE(dendrite_gpu_integrate(nullptr, 0.1f));
}

TEST_F(DendriteGPUTest, UpdateCalcium_ValidContext_Succeeds) {
    CreateDendriteContext();
    UploadTestData();

    EXPECT_TRUE(dendrite_gpu_update_calcium(dend_ctx, 0.1f));
}

TEST_F(DendriteGPUTest, DetectNMDASpikes_ValidContext_Succeeds) {
    CreateDendriteContext();
    UploadTestData();

    EXPECT_TRUE(dendrite_gpu_detect_nmda_spikes(dend_ctx));
}

TEST_F(DendriteGPUTest, ApplySTDP_ValidEvents_Succeeds) {
    CreateDendriteContext();
    UploadTestData();

    std::vector<dendrite_gpu_stdp_event_t> events(10);
    for (int i = 0; i < 10; i++) {
        events[i].spine_id = i % TEST_NUM_SPINES;
        events[i].dendrite_id = i % TEST_NUM_DENDRITES;
        events[i].pre_time = 1000 + i * 10;
        events[i].post_time = 1005 + i * 10;
        events[i].current = 0.0f;
    }

    EXPECT_TRUE(dendrite_gpu_apply_stdp(dend_ctx, events.data(), 10, 2000));
}

TEST_F(DendriteGPUTest, ApplySTDP_NullEvents_Succeeds) {
    CreateDendriteContext();
    UploadTestData();

    EXPECT_TRUE(dendrite_gpu_apply_stdp(dend_ctx, nullptr, 0, 2000));
}

TEST_F(DendriteGPUTest, PropagateBap_ValidContext_Succeeds) {
    CreateDendriteContext();
    UploadTestData();

    EXPECT_TRUE(dendrite_gpu_propagate_bap(dend_ctx, 100.0f, 0.1f));
}

TEST_F(DendriteGPUTest, ComputeAxialCurrents_ValidContext_Succeeds) {
    CreateDendriteContext();
    UploadTestData();

    EXPECT_TRUE(dendrite_gpu_compute_axial_currents(dend_ctx));
}

//=============================================================================
// Batch Input Tests
//=============================================================================

TEST_F(DendriteGPUTest, InjectCurrents_ValidInput_Succeeds) {
    CreateDendriteContext();
    UploadTestData();

    std::vector<uint32_t> spine_indices = {0, 1, 2, 3, 4};
    std::vector<uint32_t> dendrite_indices = {0, 0, 0, 1, 1};
    std::vector<float> currents = {10.0f, 20.0f, 15.0f, 10.0f, 5.0f};

    EXPECT_TRUE(dendrite_gpu_inject_currents(
        dend_ctx, spine_indices.data(), dendrite_indices.data(),
        currents.data(), 5));
}

TEST_F(DendriteGPUTest, DecayTraces_ValidContext_Succeeds) {
    CreateDendriteContext();
    UploadTestData();

    EXPECT_TRUE(dendrite_gpu_decay_traces(dend_ctx, 0.1f));
}

//=============================================================================
// Full Step Tests
//=============================================================================

TEST_F(DendriteGPUTest, Step_ValidContext_Succeeds) {
    CreateDendriteContext();
    UploadTestData();

    EXPECT_TRUE(dendrite_gpu_step(dend_ctx, 0.1f, 1000));
}

TEST_F(DendriteGPUTest, Step_MultipleSteps_Stable) {
    CreateDendriteContext();
    UploadTestData();

    for (int i = 0; i < 100; i++) {
        EXPECT_TRUE(dendrite_gpu_step(dend_ctx, 0.1f, i * 100));
    }
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(DendriteGPUTest, GetStats_ValidContext_Succeeds) {
    CreateDendriteContext();
    UploadTestData();

    dendrite_gpu_stats_t stats;
    EXPECT_TRUE(dendrite_gpu_get_stats(dend_ctx, &stats));
}

TEST_F(DendriteGPUTest, GetStats_NullStats_ReturnsFalse) {
    CreateDendriteContext();
    EXPECT_FALSE(dendrite_gpu_get_stats(dend_ctx, nullptr));
}

TEST_F(DendriteGPUTest, ResetStats_ValidContext_Succeeds) {
    CreateDendriteContext();
    UploadTestData();

    // Do some operations
    dendrite_gpu_integrate(dend_ctx, 0.1f);
    dendrite_gpu_update_calcium(dend_ctx, 0.1f);

    // Reset
    dendrite_gpu_reset_stats(dend_ctx);

    dendrite_gpu_stats_t stats;
    dendrite_gpu_get_stats(dend_ctx, &stats);
    EXPECT_EQ(stats.integrate_calls, 0u);
}

//=============================================================================
// CPU Reference Tests
//=============================================================================

TEST_F(DendriteGPUTest, CPUIntegrate_ValidData_Succeeds) {
    std::vector<float> voltages(SMALL_NETWORK * TEST_NUM_SEGMENTS, -70.0f);
    std::vector<float> cable_params(SMALL_NETWORK * 3);
    for (uint32_t i = 0; i < SMALL_NETWORK; i++) {
        cable_params[i * 3 + 0] = 10000.0f;  // rm
        cable_params[i * 3 + 1] = 1.0f;      // cm
        cable_params[i * 3 + 2] = 100.0f;    // ra
    }
    std::vector<float> axial_currents(SMALL_NETWORK * TEST_NUM_SEGMENTS, 0.0f);

    EXPECT_TRUE(dendrite_cpu_integrate(
        voltages.data(), cable_params.data(), axial_currents.data(),
        SMALL_NETWORK, TEST_NUM_SEGMENTS, 0.1f));
}

TEST_F(DendriteGPUTest, CPUUpdateCalcium_ValidData_Succeeds) {
    std::vector<float> calcium(SMALL_NETWORK * TEST_NUM_SPINES, 0.5f);
    std::vector<float> nmda_states(SMALL_NETWORK * TEST_NUM_SPINES, 0.0f);
    std::vector<float> bap_signals(SMALL_NETWORK * TEST_NUM_SPINES, 0.0f);

    EXPECT_TRUE(dendrite_cpu_update_calcium(
        calcium.data(), nmda_states.data(), bap_signals.data(),
        SMALL_NETWORK, TEST_NUM_SPINES, 0.1f, 50.0f));

    // Calcium should decay
    for (float c : calcium) {
        EXPECT_LT(c, 0.5f);
    }
}

TEST_F(DendriteGPUTest, CPUDetectNMDASpikes_ValidData_Succeeds) {
    std::vector<float> nmda_states(SMALL_NETWORK * TEST_NUM_SEGMENTS, 0.0f);
    std::vector<float> voltages(SMALL_NETWORK * TEST_NUM_SEGMENTS, -70.0f);

    // Set voltages high enough to relieve Mg2+ block (needs to be positive)
    // mg_block = 1 / (1 + exp(-0.062*V))
    // At 0mV: mg_block = 0.5 exactly (need > 0.5)
    // At +10mV: mg_block = 0.65 (above threshold)
    // At +20mV: mg_block = 0.77 (well above threshold)
    voltages[0] = 20.0f;  // Well above 0mV to ensure mg_block > 0.5
    voltages[10] = 30.0f; // Even more depolarized

    EXPECT_TRUE(dendrite_cpu_detect_nmda_spikes(
        nmda_states.data(), voltages.data(),
        SMALL_NETWORK, TEST_NUM_SEGMENTS, -40.0f));

    // NMDA states should be activated where voltage > threshold AND Mg2+ block relieved
    EXPECT_GT(nmda_states[0], 0.0f);
    EXPECT_GT(nmda_states[10], 0.0f);
}

TEST_F(DendriteGPUTest, CPUApplySTDP_ValidData_Succeeds) {
    std::vector<float> weights(SMALL_NETWORK, 0.5f);
    std::vector<float> pre_traces(SMALL_NETWORK, 0.0f);
    std::vector<float> post_traces(SMALL_NETWORK, 0.0f);

    std::vector<dendrite_gpu_stdp_event_t> events(5);
    for (int i = 0; i < 5; i++) {
        events[i].spine_id = i;
        events[i].dendrite_id = 0;
        events[i].pre_time = 1000;
        events[i].post_time = 1010;  // Post after pre -> LTP
        events[i].current = 0.0f;
    }

    EXPECT_TRUE(dendrite_cpu_apply_stdp(
        weights.data(), pre_traces.data(), post_traces.data(),
        events.data(), 5, 0.01f, 0.012f, 20.0f, 20.0f));
}

//=============================================================================
// Large Network Tests
//=============================================================================

TEST_F(DendriteGPUTest, LargeNetwork_CreateAndStep_Succeeds) {
    if (!gpu_available) GTEST_SKIP() << "GPU not available";

    dendrite_gpu_config_t config = dendrite_gpu_default_config();
    config.max_dendrites = LARGE_NETWORK * 2;
    config.max_segments_per_dendrite = TEST_NUM_SEGMENTS;
    config.max_spines_per_dendrite = TEST_NUM_SPINES;

    dend_ctx = dendrite_gpu_create(gpu_ctx, &config);
    ASSERT_NE(dend_ctx, nullptr);

    // Upload large dataset
    std::vector<dendrite_gpu_segment_t> segments(LARGE_NETWORK * TEST_NUM_SEGMENTS);
    for (auto& seg : segments) {
        seg.voltage = -70.0f;
        seg.length = 10.0f;
        seg.diameter = 1.0f;
    }
    ASSERT_TRUE(dendrite_gpu_upload_segments(
        dend_ctx, segments.data(), LARGE_NETWORK, TEST_NUM_SEGMENTS));

    // Run step
    EXPECT_TRUE(dendrite_gpu_step(dend_ctx, 0.1f, 1000));
}
