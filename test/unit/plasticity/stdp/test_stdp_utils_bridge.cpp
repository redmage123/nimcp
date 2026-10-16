//=============================================================================
// test_stdp_utils_bridge.cpp - Unit Tests for STDP Utils Bridge
//=============================================================================
/**
 * @file test_stdp_utils_bridge.cpp
 * @brief Comprehensive tests for STDP utils integration module
 *
 * Tests: Ring buffer, metrics, memory pools, RK4 integration, phase gating
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <cstring>

extern "C" {
#include "plasticity/stdp/nimcp_stdp_utils_bridge.h"
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

class STDPUtilsBridgeLifecycleTest : public ::testing::Test {
protected:
    stdp_utils_ctx_t ctx = nullptr;

    void TearDown() override {
        if (ctx) {
            stdp_utils_destroy(ctx);
            ctx = nullptr;
        }
    }
};

TEST_F(STDPUtilsBridgeLifecycleTest, CreateWithDefaultConfig) {
    ctx = stdp_utils_create(nullptr);
    ASSERT_NE(ctx, nullptr);
}

TEST_F(STDPUtilsBridgeLifecycleTest, CreateWithCustomConfig) {
    stdp_utils_config_t config = stdp_utils_default_config();
    config.enable_spike_buffering = true;
    config.enable_metrics = true;
    config.enable_synapse_pool = true;
    config.spike_history_size = 512;

    ctx = stdp_utils_create(&config);
    ASSERT_NE(ctx, nullptr);
}

TEST_F(STDPUtilsBridgeLifecycleTest, DestroyNull) {
    stdp_utils_destroy(nullptr);  // Should not crash
}

TEST_F(STDPUtilsBridgeLifecycleTest, ResetContext) {
    ctx = stdp_utils_create(nullptr);
    ASSERT_NE(ctx, nullptr);

    // Should not crash
    stdp_utils_reset(ctx);
}

TEST_F(STDPUtilsBridgeLifecycleTest, DefaultConfigValues) {
    stdp_utils_config_t config = stdp_utils_default_config();

    EXPECT_EQ(config.spike_history_size, STDP_SPIKE_HISTORY_SIZE);
    EXPECT_TRUE(config.enable_spike_buffering);
    EXPECT_TRUE(config.enable_metrics);
    EXPECT_TRUE(config.enable_synapse_pool);
}

//=============================================================================
// Spike Buffer Tests
//=============================================================================

class STDPUtilsSpikeBufferTest : public ::testing::Test {
protected:
    stdp_utils_ctx_t ctx = nullptr;

    void SetUp() override {
        stdp_utils_config_t config = stdp_utils_default_config();
        config.enable_spike_buffering = true;
        ctx = stdp_utils_create(&config);
    }

    void TearDown() override {
        if (ctx) {
            stdp_utils_destroy(ctx);
            ctx = nullptr;
        }
    }
};

TEST_F(STDPUtilsSpikeBufferTest, RecordSingleSpike) {
    stdp_spike_event_t event = {
        .source_id = 1,
        .target_id = 2,
        .timestamp = 10.0f,
        .amplitude = 1.0f,
        .is_pre = true
    };

    bool result = stdp_utils_record_spike(ctx, &event);
    EXPECT_TRUE(result);
}

TEST_F(STDPUtilsSpikeBufferTest, RecordMultipleSpikes) {
    for (int i = 0; i < 100; i++) {
        stdp_spike_event_t event = {
            .source_id = (uint32_t)i,
            .target_id = (uint32_t)(i + 1),
            .timestamp = (float)(i * 1.0),
            .amplitude = 1.0f,
            .is_pre = (i % 2 == 0)
        };

        bool result = stdp_utils_record_spike(ctx, &event);
        EXPECT_TRUE(result);
    }
}

TEST_F(STDPUtilsSpikeBufferTest, GetSpikesInWindow) {
    // Record spikes at different times
    for (int i = 0; i < 50; i++) {
        stdp_spike_event_t event = {
            .source_id = (uint32_t)i,
            .target_id = 0,
            .timestamp = (float)(i * 2.0),  // 0, 2, 4, 6, ..., 98 ms
            .amplitude = 1.0f,
            .is_pre = true
        };
        stdp_utils_record_spike(ctx, &event);
    }

    // Get spikes in window [20, 40]
    stdp_spike_event_t out_events[20];
    uint32_t num_found = 0;

    bool result = stdp_utils_get_spikes_in_window(ctx, 20.0f, 40.0f, out_events, 20, &num_found);
    EXPECT_TRUE(result);
    EXPECT_GT(num_found, 0u);

    // All returned spikes should be in the window
    for (uint32_t i = 0; i < num_found; i++) {
        EXPECT_GE(out_events[i].timestamp, 20.0f);
        EXPECT_LE(out_events[i].timestamp, 40.0f);
    }
}

TEST_F(STDPUtilsSpikeBufferTest, GetRecentSpikes) {
    // Record some spikes
    for (int i = 0; i < 20; i++) {
        stdp_spike_event_t event = {
            .source_id = (uint32_t)i,
            .target_id = 0,
            .timestamp = (float)(i * 1.0),
            .amplitude = 1.0f,
            .is_pre = true
        };
        stdp_utils_record_spike(ctx, &event);
    }

    stdp_spike_event_t out_events[10];
    uint32_t num_found = 0;

    bool result = stdp_utils_get_recent_spikes(ctx, 10, out_events, &num_found);
    EXPECT_TRUE(result);
    EXPECT_LE(num_found, 10u);
}

TEST_F(STDPUtilsSpikeBufferTest, FindSpikePairs) {
    // Record pre and post spikes for a specific pair
    stdp_spike_event_t pre_event = {
        .source_id = 100,
        .target_id = 200,
        .timestamp = 50.0f,
        .amplitude = 1.0f,
        .is_pre = true
    };
    stdp_utils_record_spike(ctx, &pre_event);

    stdp_spike_event_t post_event = {
        .source_id = 100,
        .target_id = 200,
        .timestamp = 55.0f,
        .amplitude = 1.0f,
        .is_pre = false
    };
    stdp_utils_record_spike(ctx, &post_event);

    stdp_spike_event_t out_events[10];
    uint32_t num_found = 0;

    bool result = stdp_utils_find_spike_pairs(ctx, 100, 200, 20.0f, out_events, 10, &num_found);
    EXPECT_TRUE(result);
    EXPECT_GE(num_found, 2u);  // Should find both pre and post
}

//=============================================================================
// Metrics Tests
//=============================================================================

class STDPUtilsMetricsTest : public ::testing::Test {
protected:
    stdp_utils_ctx_t ctx = nullptr;

    void SetUp() override {
        stdp_utils_config_t config = stdp_utils_default_config();
        config.enable_metrics = true;
        ctx = stdp_utils_create(&config);
    }

    void TearDown() override {
        if (ctx) {
            stdp_utils_destroy(ctx);
            ctx = nullptr;
        }
    }
};

TEST_F(STDPUtilsMetricsTest, RecordLTP) {
    stdp_utils_record_ltp(ctx, 0.01f, 5.0f);  // Weight change, timing delta
    stdp_utils_record_ltp(ctx, 0.02f, 3.0f);

    stdp_metrics_t metrics;
    bool result = stdp_utils_get_metrics(ctx, &metrics);
    EXPECT_TRUE(result);
    EXPECT_EQ(metrics.total_ltp_events, 2u);
}

TEST_F(STDPUtilsMetricsTest, RecordLTD) {
    stdp_utils_record_ltd(ctx, -0.01f, -5.0f);
    stdp_utils_record_ltd(ctx, -0.015f, -7.0f);
    stdp_utils_record_ltd(ctx, -0.02f, -4.0f);

    stdp_metrics_t metrics;
    bool result = stdp_utils_get_metrics(ctx, &metrics);
    EXPECT_TRUE(result);
    EXPECT_EQ(metrics.total_ltd_events, 3u);
}

TEST_F(STDPUtilsMetricsTest, UpdateWeightStats) {
    std::vector<float> weights = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};

    stdp_utils_update_weight_stats(ctx, weights.data(), weights.size());

    stdp_metrics_t metrics;
    bool result = stdp_utils_get_metrics(ctx, &metrics);
    EXPECT_TRUE(result);
    EXPECT_NEAR(metrics.mean_weight, 0.3f, 0.01f);
    EXPECT_GT(metrics.weight_variance, 0.0f);
}

TEST_F(STDPUtilsMetricsTest, LTPLTDRatio) {
    // Record more LTP than LTD
    for (int i = 0; i < 10; i++) {
        stdp_utils_record_ltp(ctx, 0.01f, 5.0f);
    }
    for (int i = 0; i < 5; i++) {
        stdp_utils_record_ltd(ctx, -0.01f, -5.0f);
    }

    stdp_metrics_t metrics;
    bool result = stdp_utils_get_metrics(ctx, &metrics);
    EXPECT_TRUE(result);
    EXPECT_NEAR(metrics.ltp_ltd_ratio, 2.0f, 0.1f);  // 10/5 = 2
}

//=============================================================================
// Memory Pool Tests
//=============================================================================

class STDPUtilsPoolTest : public ::testing::Test {
protected:
    stdp_utils_ctx_t ctx = nullptr;

    void SetUp() override {
        stdp_utils_config_t config = stdp_utils_default_config();
        config.enable_synapse_pool = true;
        config.synapse_pool_size = 100;
        ctx = stdp_utils_create(&config);
    }

    void TearDown() override {
        if (ctx) {
            stdp_utils_destroy(ctx);
            ctx = nullptr;
        }
    }
};

TEST_F(STDPUtilsPoolTest, AllocSingleSynapse) {
    stdp_synapse_t* synapse = stdp_utils_alloc_synapse(ctx);
    ASSERT_NE(synapse, nullptr);

    stdp_utils_free_synapse(ctx, synapse);
}

TEST_F(STDPUtilsPoolTest, AllocMultipleSynapses) {
    std::vector<stdp_synapse_t*> synapses;

    for (int i = 0; i < 50; i++) {
        stdp_synapse_t* synapse = stdp_utils_alloc_synapse(ctx);
        ASSERT_NE(synapse, nullptr);
        synapses.push_back(synapse);
    }

    // Free all
    for (auto synapse : synapses) {
        stdp_utils_free_synapse(ctx, synapse);
    }
}

TEST_F(STDPUtilsPoolTest, BatchAllocSynapses) {
    stdp_synapse_t* synapses[20];

    uint32_t allocated = stdp_utils_alloc_synapse_batch(ctx, 20, synapses);
    EXPECT_EQ(allocated, 20u);

    // Verify all are valid
    for (uint32_t i = 0; i < allocated; i++) {
        EXPECT_NE(synapses[i], nullptr);
        stdp_utils_free_synapse(ctx, synapses[i]);
    }
}

TEST_F(STDPUtilsPoolTest, PoolStats) {
    // Allocate some synapses
    stdp_synapse_t* synapses[30];
    for (int i = 0; i < 30; i++) {
        synapses[i] = stdp_utils_alloc_synapse(ctx);
    }

    uint32_t total, used, free_count;
    stdp_utils_pool_stats(ctx, &total, &used, &free_count);

    EXPECT_EQ(total, 100u);
    EXPECT_EQ(used, 30u);
    EXPECT_EQ(free_count, 70u);

    // Cleanup
    for (int i = 0; i < 30; i++) {
        stdp_utils_free_synapse(ctx, synapses[i]);
    }
}

//=============================================================================
// RK4 Integration Tests
//=============================================================================

class STDPRK4Test : public ::testing::Test {
protected:
    stdp_utils_ctx_t ctx = nullptr;

    void SetUp() override {
        stdp_utils_config_t config = stdp_utils_default_config();
        config.use_rk4_trace_decay = true;
        ctx = stdp_utils_create(&config);
    }

    void TearDown() override {
        if (ctx) {
            stdp_utils_destroy(ctx);
            ctx = nullptr;
        }
    }
};

TEST_F(STDPRK4Test, TraceDecayNoSpike) {
    float trace = 1.0f;
    float tau = 20.0f;  // 20ms time constant
    float dt = 1.0f;    // 1ms timestep

    stdp_utils_rk4_trace_update(&trace, tau, dt, 0.0f);

    // Trace should decay
    EXPECT_LT(trace, 1.0f);
    EXPECT_GT(trace, 0.0f);

    // Expected: e^(-dt/tau) = e^(-1/20) ≈ 0.951
    EXPECT_NEAR(trace, std::exp(-dt/tau), 0.01f);
}

TEST_F(STDPRK4Test, TraceDecayWithSpike) {
    float trace = 0.5f;
    float tau = 20.0f;
    float dt = 1.0f;

    stdp_utils_rk4_trace_update(&trace, tau, dt, 1.0f);  // Spike contribution

    // Trace should increase due to spike
    EXPECT_GT(trace, 0.5f);
}

TEST_F(STDPRK4Test, BatchTraceUpdate) {
    std::vector<float> traces = {1.0f, 0.8f, 0.6f, 0.4f, 0.2f};
    std::vector<float> taus(5, 20.0f);
    uint8_t spike_mask[1] = {0b00101};  // Spikes at indices 0 and 2

    stdp_utils_rk4_trace_batch(traces.data(), taus.data(), 5, 1.0f, spike_mask);

    // Traces with spikes should increase, others decay
    EXPECT_GT(traces[0], 0.9f);  // Had spike
    EXPECT_LT(traces[1], 0.8f);  // No spike, decayed
    EXPECT_GT(traces[2], 0.5f);  // Had spike
}

//=============================================================================
// Phase Gating Tests
//=============================================================================

class STDPPhaseGatingTest : public ::testing::Test {
protected:
    stdp_utils_ctx_t ctx = nullptr;

    void SetUp() override {
        stdp_utils_config_t config = stdp_utils_default_config();
        config.enable_phase_gating = true;
        config.encoding_phase_start = 0.0f;    // Encoding starts at 0 degrees
        config.encoding_phase_end = 180.0f;    // Encoding ends at 180 degrees
        ctx = stdp_utils_create(&config);
    }

    void TearDown() override {
        if (ctx) {
            stdp_utils_destroy(ctx);
            ctx = nullptr;
        }
    }
};

TEST_F(STDPPhaseGatingTest, InEncodingWindowTrue) {
    // 90 degrees is in [0, 180]
    bool in_window = stdp_utils_in_encoding_window(ctx, 90.0f);
    EXPECT_TRUE(in_window);
}

TEST_F(STDPPhaseGatingTest, InEncodingWindowFalse) {
    // 270 degrees is NOT in [0, 180]
    bool in_window = stdp_utils_in_encoding_window(ctx, 270.0f);
    EXPECT_FALSE(in_window);
}

TEST_F(STDPPhaseGatingTest, PhaseModulationInWindow) {
    // Should have full modulation in encoding window
    float mod = stdp_utils_phase_modulation(ctx, 90.0f);
    EXPECT_GT(mod, 0.5f);
}

TEST_F(STDPPhaseGatingTest, PhaseModulationOutWindow) {
    // Should have reduced modulation outside encoding window
    float mod = stdp_utils_phase_modulation(ctx, 270.0f);
    EXPECT_LT(mod, 0.5f);
}

//=============================================================================
// Enhanced Operations Tests
//=============================================================================

class STDPUtilsEnhancedTest : public ::testing::Test {
protected:
    stdp_utils_ctx_t ctx = nullptr;

    void SetUp() override {
        stdp_utils_config_t config = stdp_utils_default_config();
        config.enable_spike_buffering = true;
        config.enable_metrics = true;
        config.enable_synapse_pool = true;
        config.use_rk4_trace_decay = true;
        ctx = stdp_utils_create(&config);
    }

    void TearDown() override {
        if (ctx) {
            stdp_utils_destroy(ctx);
            ctx = nullptr;
        }
    }
};

TEST_F(STDPUtilsEnhancedTest, PreSpikeEnhanced) {
    stdp_synapse_t* synapse = stdp_utils_alloc_synapse(ctx);
    ASSERT_NE(synapse, nullptr);

    // Initialize synapse
    synapse->weight = 0.5f;
    synapse->pre_trace = 0.0f;
    synapse->post_trace = 0.5f;  // Some prior post activity

    float weight_change = stdp_utils_pre_spike_enhanced(ctx, synapse, 100.0f, 90.0f);

    // Pre-trace should have increased
    EXPECT_GT(synapse->pre_trace, 0.0f);

    stdp_utils_free_synapse(ctx, synapse);
}

TEST_F(STDPUtilsEnhancedTest, PostSpikeEnhanced) {
    stdp_synapse_t* synapse = stdp_utils_alloc_synapse(ctx);
    ASSERT_NE(synapse, nullptr);

    // Initialize synapse with prior pre activity
    synapse->weight = 0.5f;
    synapse->pre_trace = 0.5f;
    synapse->post_trace = 0.0f;

    float weight_change = stdp_utils_post_spike_enhanced(ctx, synapse, 100.0f, 90.0f);

    // Post-trace should have increased
    EXPECT_GT(synapse->post_trace, 0.0f);

    stdp_utils_free_synapse(ctx, synapse);
}

TEST_F(STDPUtilsEnhancedTest, BatchProcess) {
    // Create contiguous array of synapses for batch processing
    std::vector<stdp_synapse_t> synapses(5);

    // Initialize
    for (int i = 0; i < 5; i++) {
        stdp_synapse_init(&synapses[i]);
        synapses[i].weight = 0.5f;
        synapses[i].pre_trace = 0.1f;
        synapses[i].post_trace = 0.1f;
    }

    // Create spike events
    std::vector<stdp_spike_event_t> events(5);
    for (int i = 0; i < 5; i++) {
        events[i].source_id = i;
        events[i].target_id = i;
        events[i].timestamp = 100.0f + i;
        events[i].amplitude = 1.0f;
        events[i].is_pre = (i % 2 == 0);
    }

    float weight_changes[5];
    uint32_t processed = stdp_utils_batch_process(ctx, synapses.data(), events.data(), 5, 90.0f, weight_changes);

    EXPECT_EQ(processed, 5u);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
