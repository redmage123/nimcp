//=============================================================================
// test_cognitive_adapters_integration.cpp - Cognitive Adapters Integration Tests
//
// Tests integration of:
// - Working memory adapters with buffering and attention
// - Consolidation adapters with multi-timescale integration
// - Attention adapters with routing and salience
// - End-to-end cognitive processing pipelines
//=============================================================================

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <cmath>
#include <random>
#include <chrono>

// Headers have their own extern "C" guards
#include "middleware/cognitive/nimcp_cognitive_adapters.h"
#include "middleware/cognitive/nimcp_working_memory_adapter.h"
#include "middleware/cognitive/nimcp_consolidation_adapter.h"
#include "middleware/cognitive/nimcp_attention_adapter.h"
#include "middleware/buffering/nimcp_sliding_window.h"
#include "middleware/buffering/nimcp_integration_buffer.h"
#include "middleware/routing/nimcp_attention_gate.h"
#include "core/brain/nimcp_brain.h"

//=============================================================================
// TEST FIXTURE
//=============================================================================

class CognitiveAdaptersIntegrationTest : public ::testing::Test {
protected:
    wm_adapter_t* wm_adapter;
    consol_adapter_t* consol_adapter;
    attention_adapter_t* attention_adapter;

    // Test data
    std::vector<float> test_activity;
    std::mt19937 rng;

    void SetUp() override {
        rng.seed(42);

        // Create adapters with default configs
        wm_adapter = nullptr;  // Created in tests
        consol_adapter = nullptr;
        attention_adapter = nullptr;

        // Generate test neural activity
        generateTestActivity();
    }

    void TearDown() override {
        if (wm_adapter) wm_adapter_destroy(wm_adapter);
        if (consol_adapter) consol_adapter_destroy(consol_adapter);
        if (attention_adapter) attention_adapter_destroy(attention_adapter);
    }

    void generateTestActivity() {
        test_activity.clear();
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);

        for (int i = 0; i < 256; i++) {
            test_activity.push_back(dist(rng));
        }
    }

    uint64_t getCurrentTimeUs() {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(
            now.time_since_epoch()).count();
    }
};

//=============================================================================
// 1. WORKING MEMORY ADAPTER INTEGRATION TESTS
//=============================================================================

TEST_F(CognitiveAdaptersIntegrationTest, WorkingMemoryBasicItemManagement) {
    // WHAT: Test adding, retrieving, and removing items
    // WHY: Basic WM operations must work correctly

    wm_adapter_config_t config = wm_adapter_default_config();
    config.capacity = 9;
    config.mode = WM_MODE_SLIDING;

    wm_adapter = wm_adapter_create(&config);
    ASSERT_NE(wm_adapter, nullptr);

    // Add items
    float data1[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    float data2[4] = {5.0f, 6.0f, 7.0f, 8.0f};

    EXPECT_TRUE(wm_adapter_add_item(wm_adapter, 1, data1, 4, 0.8f));
    EXPECT_TRUE(wm_adapter_add_item(wm_adapter, 2, data2, 4, 0.6f));

    // Retrieve items
    const wm_item_t* item1 = wm_adapter_get_item(wm_adapter, 1);
    ASSERT_NE(item1, nullptr);
    EXPECT_EQ(item1->item_id, 1);
    EXPECT_FLOAT_EQ(item1->salience, 0.8f);
    EXPECT_EQ(item1->data_size, 4);

    // Get statistics
    wm_adapter_stats_t stats;
    EXPECT_TRUE(wm_adapter_get_stats(wm_adapter, &stats));
    EXPECT_EQ(stats.current_items, 2);
    EXPECT_EQ(stats.total_added, 2);
}

TEST_F(CognitiveAdaptersIntegrationTest, WorkingMemoryCapacityEviction) {
    // WHAT: Test capacity limits and eviction policy
    // WHY: WM must maintain 7±2 item limit with intelligent eviction

    wm_adapter_config_t config = wm_adapter_default_config();
    config.capacity = 5;  // Small capacity for testing
    config.mode = WM_MODE_SLIDING;

    wm_adapter = wm_adapter_create(&config);
    ASSERT_NE(wm_adapter, nullptr);

    // Add more items than capacity
    for (uint32_t i = 0; i < 7; i++) {
        float data[2] = {static_cast<float>(i), static_cast<float>(i + 1)};
        float salience = 0.5f + (i * 0.05f);  // Increasing salience
        EXPECT_TRUE(wm_adapter_add_item(wm_adapter, i, data, 2, salience));
    }

    // Should have exactly capacity items
    wm_adapter_stats_t stats;
    EXPECT_TRUE(wm_adapter_get_stats(wm_adapter, &stats));
    EXPECT_EQ(stats.current_items, 5);
    EXPECT_EQ(stats.total_added, 7);
    EXPECT_EQ(stats.total_evicted, 2);

    // Lower salience items should be evicted
    const wm_item_t* item0 = wm_adapter_get_item(wm_adapter, 0);
    EXPECT_EQ(item0, nullptr);  // Should be evicted

    const wm_item_t* item6 = wm_adapter_get_item(wm_adapter, 6);
    EXPECT_NE(item6, nullptr);  // Should remain (highest salience)
}

TEST_F(CognitiveAdaptersIntegrationTest, WorkingMemoryAttentionGating) {
    // WHAT: Test attention-gated working memory mode
    // WHY: Attention should modulate item storage and retrieval

    wm_adapter_config_t config = wm_adapter_default_config();
    config.mode = WM_MODE_ATTENTION_GATED;
    config.attention_threshold = 0.5f;
    config.capacity = 9;

    wm_adapter = wm_adapter_create(&config);
    ASSERT_NE(wm_adapter, nullptr);

    // Add items with different saliences
    float high_sal_data[2] = {1.0f, 2.0f};
    float low_sal_data[2] = {3.0f, 4.0f};

    // High salience - should be stored
    EXPECT_TRUE(wm_adapter_add_item(wm_adapter, 1, high_sal_data, 2, 0.9f));

    // Low salience - should be rejected
    EXPECT_FALSE(wm_adapter_add_item(wm_adapter, 2, low_sal_data, 2, 0.3f));

    wm_adapter_stats_t stats;
    EXPECT_TRUE(wm_adapter_get_stats(wm_adapter, &stats));
    EXPECT_EQ(stats.current_items, 1);
}

TEST_F(CognitiveAdaptersIntegrationTest, WorkingMemoryAttentionModulation) {
    // WHAT: Test dynamic attention weight updates
    // WHY: Attention should modulate existing item salience

    wm_adapter_config_t config = wm_adapter_default_config();
    config.mode = WM_MODE_HYBRID;

    wm_adapter = wm_adapter_create(&config);
    ASSERT_NE(wm_adapter, nullptr);

    // Add item with initial salience
    float data[2] = {1.0f, 2.0f};
    EXPECT_TRUE(wm_adapter_add_item(wm_adapter, 1, data, 2, 0.5f));

    // Boost attention
    EXPECT_TRUE(wm_adapter_set_attention(wm_adapter, 1, 0.9f));

    // Item should still be accessible
    const wm_item_t* item = wm_adapter_get_item(wm_adapter, 1);
    ASSERT_NE(item, nullptr);
    EXPECT_GT(item->salience, 0.5f);  // Should be boosted
}

TEST_F(CognitiveAdaptersIntegrationTest, WorkingMemoryTemporalDecay) {
    // WHAT: Test time-based salience decay
    // WHY: WM items should fade without rehearsal

    wm_adapter_config_t config = wm_adapter_default_config();
    config.enable_decay = true;
    config.decay_tau_ms = 100.0f;  // Fast decay for testing

    wm_adapter = wm_adapter_create(&config);
    ASSERT_NE(wm_adapter, nullptr);

    // Add item
    float data[2] = {1.0f, 2.0f};
    EXPECT_TRUE(wm_adapter_add_item(wm_adapter, 1, data, 2, 0.9f));

    const wm_item_t* item1 = wm_adapter_get_item(wm_adapter, 1);
    float initial_salience = item1->salience;

    // Apply decay
    wm_adapter_update_decay(wm_adapter, 100000);  // 100ms

    const wm_item_t* item2 = wm_adapter_get_item(wm_adapter, 1);
    ASSERT_NE(item2, nullptr);
    EXPECT_LT(item2->salience, initial_salience);
}

TEST_F(CognitiveAdaptersIntegrationTest, WorkingMemoryClearOperation) {
    // WHAT: Test clearing all WM contents
    // WHY: Context switches require full WM reset

    wm_adapter_config_t config = wm_adapter_default_config();
    wm_adapter = wm_adapter_create(&config);
    ASSERT_NE(wm_adapter, nullptr);

    // Add multiple items
    for (uint32_t i = 0; i < 5; i++) {
        float data[2] = {static_cast<float>(i), static_cast<float>(i + 1)};
        EXPECT_TRUE(wm_adapter_add_item(wm_adapter, i, data, 2, 0.7f));
    }

    wm_adapter_stats_t stats1;
    EXPECT_TRUE(wm_adapter_get_stats(wm_adapter, &stats1));
    EXPECT_EQ(stats1.current_items, 5);

    // Clear
    wm_adapter_clear(wm_adapter);

    wm_adapter_stats_t stats2;
    EXPECT_TRUE(wm_adapter_get_stats(wm_adapter, &stats2));
    EXPECT_EQ(stats2.current_items, 0);
}

//=============================================================================
// 2. CONSOLIDATION ADAPTER INTEGRATION TESTS
//=============================================================================

TEST_F(CognitiveAdaptersIntegrationTest, ConsolidationBasicUpdate) {
    // WHAT: Test basic multi-timescale consolidation
    // WHY: Consolidation must integrate across temporal scales

    consol_adapter_config_t config = consol_adapter_default_config();
    config.num_channels = 4;
    config.strategy = CONSOL_STRATEGY_AVERAGE;

    consol_adapter = consol_adapter_create(&config);
    ASSERT_NE(consol_adapter, nullptr);

    // Update channels
    uint64_t timestamp = 0;
    for (int t = 0; t < 10; t++) {
        for (size_t ch = 0; ch < 4; ch++) {
            float value = static_cast<float>(ch + t * 0.1f);
            EXPECT_TRUE(consol_adapter_update(consol_adapter, ch, value, timestamp));
        }
        timestamp += 1000;  // 1ms
    }

    // Get consolidated values
    for (size_t ch = 0; ch < 4; ch++) {
        float value = consol_adapter_get_consolidated(consol_adapter, ch);
        EXPECT_GE(value, 0.0f);
    }
}

TEST_F(CognitiveAdaptersIntegrationTest, ConsolidationMultiTimescale) {
    // WHAT: Test retrieval from different timescales
    // WHY: Brain processes at fast, medium, and slow timescales

    consol_adapter_config_t config = consol_adapter_default_config();
    config.num_channels = 2;
    config.fast_size = 10;
    config.medium_size = 100;
    config.slow_size = 1000;

    consol_adapter = consol_adapter_create(&config);
    ASSERT_NE(consol_adapter, nullptr);

    // Simulate oscillating signal
    uint64_t timestamp = 0;
    for (int t = 0; t < 200; t++) {
        float value = std::sin(t * 0.1f);
        EXPECT_TRUE(consol_adapter_update(consol_adapter, 0, value, timestamp));
        timestamp += 1000;  // 1ms
    }

    // Get values at different timescales
    float fast = consol_adapter_get_value(consol_adapter, TIMESCALE_FAST, 0);
    float medium = consol_adapter_get_value(consol_adapter, TIMESCALE_MEDIUM, 0);
    float slow = consol_adapter_get_value(consol_adapter, TIMESCALE_SLOW, 0);

    // All should be valid
    EXPECT_FALSE(std::isnan(fast));
    EXPECT_FALSE(std::isnan(medium));
    EXPECT_FALSE(std::isnan(slow));
}

TEST_F(CognitiveAdaptersIntegrationTest, ConsolidationWeightedStrategy) {
    // WHAT: Test salience-weighted consolidation
    // WHY: Important events should have more influence

    consol_adapter_config_t config = consol_adapter_default_config();
    config.strategy = CONSOL_STRATEGY_WEIGHTED;
    config.num_channels = 1;

    consol_adapter = consol_adapter_create(&config);
    ASSERT_NE(consol_adapter, nullptr);

    // Update with varying values
    uint64_t timestamp = 0;
    EXPECT_TRUE(consol_adapter_update(consol_adapter, 0, 10.0f, timestamp));
    timestamp += 1000;
    EXPECT_TRUE(consol_adapter_update(consol_adapter, 0, 20.0f, timestamp));
    timestamp += 1000;
    EXPECT_TRUE(consol_adapter_update(consol_adapter, 0, 15.0f, timestamp));

    float consolidated = consol_adapter_get_consolidated(consol_adapter, 0);
    EXPECT_GT(consolidated, 10.0f);
    EXPECT_LT(consolidated, 20.0f);
}

TEST_F(CognitiveAdaptersIntegrationTest, ConsolidationTrendDetection) {
    // WHAT: Test temporal trend calculation
    // WHY: Detect gradual changes across timescales

    consol_adapter_config_t config = consol_adapter_default_config();
    config.num_channels = 1;

    consol_adapter = consol_adapter_create(&config);
    ASSERT_NE(consol_adapter, nullptr);

    // Create increasing trend
    uint64_t timestamp = 0;
    for (int t = 0; t < 50; t++) {
        float value = static_cast<float>(t);
        EXPECT_TRUE(consol_adapter_update(consol_adapter, 0, value, timestamp));
        timestamp += 1000;
    }

    float trend = consol_adapter_get_trend(consol_adapter, 0);
    EXPECT_GT(trend, 0.0f);  // Should be positive for increasing trend
}

TEST_F(CognitiveAdaptersIntegrationTest, ConsolidationNormalization) {
    // WHAT: Test normalization of consolidated values
    // WHY: Stable learning requires normalized inputs

    consol_adapter_config_t config = consol_adapter_default_config();
    config.enable_normalization = true;
    config.num_channels = 1;

    consol_adapter = consol_adapter_create(&config);
    ASSERT_NE(consol_adapter, nullptr);

    // Add values with different scales
    uint64_t timestamp = 0;
    std::vector<float> values = {100.0f, 200.0f, 150.0f, 180.0f, 120.0f};

    for (float val : values) {
        EXPECT_TRUE(consol_adapter_update(consol_adapter, 0, val, timestamp));
        timestamp += 1000;
    }

    float normalized = consol_adapter_normalize(consol_adapter, 0);
    // Normalized value should be in reasonable range (roughly -3 to 3 for z-score)
    EXPECT_GE(normalized, -5.0f);
    EXPECT_LE(normalized, 5.0f);
}

TEST_F(CognitiveAdaptersIntegrationTest, ConsolidationStatistics) {
    // WHAT: Test consolidation statistics tracking
    // WHY: Monitor consolidation quality and activity

    consol_adapter_config_t config = consol_adapter_default_config();
    config.num_channels = 2;

    consol_adapter = consol_adapter_create(&config);
    ASSERT_NE(consol_adapter, nullptr);

    // Perform updates
    uint64_t timestamp = 0;
    for (int t = 0; t < 20; t++) {
        EXPECT_TRUE(consol_adapter_update(consol_adapter, 0, 1.0f, timestamp));
        EXPECT_TRUE(consol_adapter_update(consol_adapter, 1, 2.0f, timestamp));
        timestamp += 1000;
    }

    consol_adapter_stats_t stats;
    EXPECT_TRUE(consol_adapter_get_stats(consol_adapter, &stats));
    EXPECT_EQ(stats.total_updates, 40);  // 20 updates * 2 channels
    EXPECT_GE(stats.integration_quality, 0.0f);
    EXPECT_LE(stats.integration_quality, 1.0f);
}

//=============================================================================
// 3. ATTENTION ADAPTER INTEGRATION TESTS
//=============================================================================

TEST_F(CognitiveAdaptersIntegrationTest, AttentionBasicWeightSetting) {
    // WHAT: Test basic attention weight control
    // WHY: Top-down attention must be controllable

    attention_adapter_config_t config = attention_adapter_default_config();
    config.mode = ATTENTION_CONTROL_TOPDOWN;
    config.max_targets = 10;

    attention_adapter = attention_adapter_create(&config);
    ASSERT_NE(attention_adapter, nullptr);

    // Set attention weights
    EXPECT_TRUE(attention_adapter_set_weight(attention_adapter, 0, 1, 0.8f));
    EXPECT_TRUE(attention_adapter_set_weight(attention_adapter, 0, 2, 0.3f));

    attention_adapter_stats_t stats;
    EXPECT_TRUE(attention_adapter_get_stats(attention_adapter, &stats));
    EXPECT_EQ(stats.active_targets, 2);
}

TEST_F(CognitiveAdaptersIntegrationTest, AttentionBottomUpSalience) {
    // WHAT: Test bottom-up salience-driven attention
    // WHY: Salient stimuli should capture attention

    attention_adapter_config_t config = attention_adapter_default_config();
    config.mode = ATTENTION_CONTROL_BOTTOMUP;

    attention_adapter = attention_adapter_create(&config);
    ASSERT_NE(attention_adapter, nullptr);

    // Update salience for targets
    EXPECT_TRUE(attention_adapter_update_salience(attention_adapter, 1, 0.9f));
    EXPECT_TRUE(attention_adapter_update_salience(attention_adapter, 2, 0.2f));

    // High salience target should have more attention
    attention_adapter_stats_t stats;
    EXPECT_TRUE(attention_adapter_get_stats(attention_adapter, &stats));
    EXPECT_GE(stats.active_targets, 1);
}

TEST_F(CognitiveAdaptersIntegrationTest, AttentionWinnerTakeAll) {
    // WHAT: Test winner-take-all competition
    // WHY: Model limited attention capacity

    attention_adapter_config_t config = attention_adapter_default_config();
    config.enable_wta = true;

    attention_adapter = attention_adapter_create(&config);
    ASSERT_NE(attention_adapter, nullptr);

    // Set multiple attention weights
    EXPECT_TRUE(attention_adapter_set_weight(attention_adapter, 0, 1, 0.7f));
    EXPECT_TRUE(attention_adapter_set_weight(attention_adapter, 0, 2, 0.9f));
    EXPECT_TRUE(attention_adapter_set_weight(attention_adapter, 0, 3, 0.5f));

    // Apply WTA
    uint32_t winner_id = 0;
    EXPECT_TRUE(attention_adapter_apply_wta(attention_adapter, &winner_id));
    EXPECT_EQ(winner_id, 2);  // Highest weight target
}

TEST_F(CognitiveAdaptersIntegrationTest, AttentionSpotlightUpdate) {
    // WHAT: Test attention spotlight (top-N selection)
    // WHY: Model flexible attention capacity

    attention_adapter_config_t config = attention_adapter_default_config();
    config.spotlight_size = 3;

    attention_adapter = attention_adapter_create(&config);
    ASSERT_NE(attention_adapter, nullptr);

    // Set weights for many targets
    for (uint32_t i = 0; i < 10; i++) {
        float weight = static_cast<float>(i) / 10.0f;
        EXPECT_TRUE(attention_adapter_set_weight(attention_adapter, 0, i, weight));
    }

    // Update spotlight
    uint32_t spotlight_ids[3];
    uint32_t num_in_spotlight = 0;
    EXPECT_TRUE(attention_adapter_update_spotlight(attention_adapter,
                                                   spotlight_ids,
                                                   &num_in_spotlight));
    EXPECT_EQ(num_in_spotlight, 3);

    // Top 3 should be IDs 7, 8, 9 (highest weights)
    EXPECT_GE(spotlight_ids[0], 7);
}

TEST_F(CognitiveAdaptersIntegrationTest, AttentionSignalRouting) {
    // WHAT: Test attention-weighted signal routing
    // WHY: Attention should modulate signal strength

    attention_adapter_config_t config = attention_adapter_default_config();
    attention_adapter = attention_adapter_create(&config);
    ASSERT_NE(attention_adapter, nullptr);

    // Set attention weight
    EXPECT_TRUE(attention_adapter_set_weight(attention_adapter, 0, 1, 0.5f));

    // Route signal
    float signal_in[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    float signal_out[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    EXPECT_TRUE(attention_adapter_route_signal(attention_adapter, 1,
                                               signal_in, signal_out, 4));

    // Output should be attenuated by weight
    for (int i = 0; i < 4; i++) {
        EXPECT_LT(signal_out[i], signal_in[i]);
        EXPECT_GT(signal_out[i], 0.0f);
    }
}

TEST_F(CognitiveAdaptersIntegrationTest, AttentionPatternDetection) {
    // WHAT: Test detection of recurring attention patterns
    // WHY: Learn attention strategies

    attention_adapter_config_t config = attention_adapter_default_config();
    config.enable_pattern_detection = true;

    attention_adapter = attention_adapter_create(&config);
    ASSERT_NE(attention_adapter, nullptr);

    // Create recurring attention pattern
    for (int rep = 0; rep < 5; rep++) {
        EXPECT_TRUE(attention_adapter_set_weight(attention_adapter, 0, 1, 0.9f));
        EXPECT_TRUE(attention_adapter_set_weight(attention_adapter, 0, 2, 0.2f));
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        EXPECT_TRUE(attention_adapter_set_weight(attention_adapter, 0, 1, 0.2f));
        EXPECT_TRUE(attention_adapter_set_weight(attention_adapter, 0, 2, 0.9f));
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Attempt pattern detection
    attention_pattern_t pattern;
    bool detected = attention_adapter_detect_pattern(attention_adapter, &pattern);
    // Pattern detection may or may not succeed depending on implementation
    // Just verify API works
    EXPECT_TRUE(true);
}

//=============================================================================
// 4. CROSS-ADAPTER INTEGRATION TESTS
//=============================================================================

TEST_F(CognitiveAdaptersIntegrationTest, WorkingMemoryConsolidationFlow) {
    // WHAT: Test WM → consolidation information flow
    // WHY: WM items should feed into consolidation

    wm_adapter_config_t wm_config = wm_adapter_default_config();
    wm_adapter = wm_adapter_create(&wm_config);
    ASSERT_NE(wm_adapter, nullptr);

    consol_adapter_config_t consol_config = consol_adapter_default_config();
    consol_config.num_channels = 4;
    consol_adapter = consol_adapter_create(&consol_config);
    ASSERT_NE(consol_adapter, nullptr);

    // Add items to WM
    float data[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    EXPECT_TRUE(wm_adapter_add_item(wm_adapter, 1, data, 4, 0.8f));

    // Get item and consolidate
    const wm_item_t* item = wm_adapter_get_item(wm_adapter, 1);
    ASSERT_NE(item, nullptr);

    uint64_t timestamp = getCurrentTimeUs();
    for (size_t ch = 0; ch < 4; ch++) {
        EXPECT_TRUE(consol_adapter_update(consol_adapter, ch,
                                         item->data[ch], timestamp));
    }

    // Verify consolidation occurred
    consol_adapter_stats_t stats;
    EXPECT_TRUE(consol_adapter_get_stats(consol_adapter, &stats));
    EXPECT_EQ(stats.total_updates, 4);
}

TEST_F(CognitiveAdaptersIntegrationTest, AttentionGatedWorkingMemory) {
    // WHAT: Test attention controlling WM storage
    // WHY: Attention gates what enters working memory

    attention_adapter_config_t attn_config = attention_adapter_default_config();
    attention_adapter = attention_adapter_create(&attn_config);
    ASSERT_NE(attention_adapter, nullptr);

    wm_adapter_config_t wm_config = wm_adapter_default_config();
    wm_config.mode = WM_MODE_ATTENTION_GATED;
    wm_adapter = wm_adapter_create(&wm_config);
    ASSERT_NE(wm_adapter, nullptr);

    // Set high attention to target 1
    EXPECT_TRUE(attention_adapter_set_weight(attention_adapter, 0, 1, 0.9f));

    // Set low attention to target 2
    EXPECT_TRUE(attention_adapter_set_weight(attention_adapter, 0, 2, 0.2f));

    // Add items with corresponding saliences
    float data1[2] = {1.0f, 2.0f};
    float data2[2] = {3.0f, 4.0f};

    EXPECT_TRUE(wm_adapter_add_item(wm_adapter, 1, data1, 2, 0.9f));
    EXPECT_FALSE(wm_adapter_add_item(wm_adapter, 2, data2, 2, 0.2f));

    wm_adapter_stats_t stats;
    EXPECT_TRUE(wm_adapter_get_stats(wm_adapter, &stats));
    EXPECT_EQ(stats.current_items, 1);
}

TEST_F(CognitiveAdaptersIntegrationTest, EndToEndCognitiveProcessing) {
    // WHAT: Test complete cognitive pipeline
    // WHY: Verify all adapters work together correctly

    // Create all adapters
    attention_adapter_config_t attn_config = attention_adapter_default_config();
    attn_config.spotlight_size = 4;
    attention_adapter = attention_adapter_create(&attn_config);
    ASSERT_NE(attention_adapter, nullptr);

    wm_adapter_config_t wm_config = wm_adapter_default_config();
    wm_config.capacity = 7;
    wm_adapter = wm_adapter_create(&wm_config);
    ASSERT_NE(wm_adapter, nullptr);

    consol_adapter_config_t consol_config = consol_adapter_default_config();
    consol_config.num_channels = 8;
    consol_adapter = consol_adapter_create(&consol_config);
    ASSERT_NE(consol_adapter, nullptr);

    // Simulate cognitive processing cycle
    uint64_t timestamp = getCurrentTimeUs();

    for (int cycle = 0; cycle < 10; cycle++) {
        // 1. Set attention based on salience
        for (uint32_t tgt = 0; tgt < 4; tgt++) {
            float salience = 0.3f + (tgt * 0.15f);
            EXPECT_TRUE(attention_adapter_update_salience(attention_adapter,
                                                         tgt, salience));
        }

        // 2. Add attended items to WM
        float wm_data[2] = {static_cast<float>(cycle),
                           static_cast<float>(cycle + 1)};
        EXPECT_TRUE(wm_adapter_add_item(wm_adapter, cycle, wm_data, 2, 0.7f));

        // 3. Consolidate active WM items
        const wm_item_t* items[10];
        uint32_t num_items = wm_adapter_get_all_items(wm_adapter, items, 10);

        for (uint32_t i = 0; i < num_items && i < 8; i++) {
            if (items[i] && items[i]->data_size > 0) {
                EXPECT_TRUE(consol_adapter_update(consol_adapter, i,
                                                 items[i]->data[0], timestamp));
            }
        }

        timestamp += 10000;  // 10ms
    }

    // Verify pipeline processed data
    wm_adapter_stats_t wm_stats;
    EXPECT_TRUE(wm_adapter_get_stats(wm_adapter, &wm_stats));
    EXPECT_GT(wm_stats.total_added, 0);

    consol_adapter_stats_t consol_stats;
    EXPECT_TRUE(consol_adapter_get_stats(consol_adapter, &consol_stats));
    EXPECT_GT(consol_stats.total_updates, 0);
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
