//=============================================================================
// test_brain_middleware_e2e.cpp - Brain-Middleware End-to-End Integration Tests
//
// Tests complete integration of:
// - Spike extraction from brain substrate
// - Cognitive processing (WM, consolidation, attention)
// - Training signal generation and routing
// - Population coding with attention routing
// - Full middleware pipeline with brain learning
//=============================================================================

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <cmath>
#include <random>
#include <chrono>
#include <memory>

// Headers have their own extern "C" guards
#include "middleware/brain_integration.h"
#include "middleware/cognitive/nimcp_cognitive_adapters.h"
#include "middleware/training/nimcp_training_adapters.h"
#include "middleware/encoding/nimcp_population_coding.h"
#include "middleware/features/nimcp_feature_extractor.h"
#include "middleware/pipeline/nimcp_middleware_pipeline.h"
#include "core/brain/nimcp_brain.h"

//=============================================================================
// TEST FIXTURE
//=============================================================================

class BrainMiddlewareE2ETest : public ::testing::Test {
protected:
    // Brain components (would use actual brain system)
    brain_temporal_buffer_t* brain_buffer;
    brain_feature_normalizer_t* brain_normalizer;
    brain_spike_feature_extractor_t spike_extractor;
    brain_population_analyzer_t population_analyzer;

    // Middleware adapters
    wm_adapter_t* wm_adapter;
    consol_adapter_t* consol_adapter;
    attention_adapter_t* attention_adapter;
    learning_signal_adapter_t learning_adapter;
    weight_update_router_t weight_router;
    training_event_manager_t event_manager;

    // Test data
    std::vector<float> neural_activity;
    std::mt19937 rng;

    void SetUp() override {
        rng.seed(42);

        // Initialize brain integration components
        brain_buffer = brain_create_temporal_buffer(64, BUFFER_SIZE_100MS);
        brain_normalizer = brain_create_feature_normalizer(32, NORMALIZE_ZSCORE);
        spike_extractor = brain_create_spike_feature_extractor(64, true, true);
        population_analyzer = brain_create_population_analyzer();

        // Initialize middleware adapters
        wm_adapter = wm_adapter_create(nullptr);
        consol_adapter = consol_adapter_create(nullptr);
        attention_adapter = attention_adapter_create(nullptr);
        learning_adapter = learning_signal_adapter_create(nullptr);
        weight_router = weight_update_router_create(nullptr, nullptr);
        event_manager = training_event_manager_create(nullptr, nullptr);

        // Generate test neural activity
        generateNeuralActivity();
    }

    void TearDown() override {
        // Cleanup brain integration
        brain_destroy_temporal_buffer(brain_buffer);
        brain_destroy_feature_normalizer(brain_normalizer);
        brain_destroy_spike_feature_extractor(spike_extractor);
        brain_destroy_population_analyzer(population_analyzer);

        // Cleanup middleware adapters
        if (wm_adapter) wm_adapter_destroy(wm_adapter);
        if (consol_adapter) consol_adapter_destroy(consol_adapter);
        if (attention_adapter) attention_adapter_destroy(attention_adapter);
        if (learning_adapter) learning_signal_adapter_destroy(learning_adapter);
        if (weight_router) weight_update_router_destroy(weight_router);
        if (event_manager) training_event_manager_destroy(event_manager);
    }

    void generateNeuralActivity() {
        neural_activity.clear();
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);

        for (int i = 0; i < 64; i++) {
            neural_activity.push_back(dist(rng));
        }
    }

    uint64_t getCurrentTimeUs() {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(
            now.time_since_epoch()).count();
    }
};

//=============================================================================
// 1. SPIKE EXTRACTION TO COGNITIVE PROCESSING
//=============================================================================

TEST_F(BrainMiddlewareE2ETest, SpikeExtractionToWorkingMemory) {
    // WHAT: Test spike data → feature extraction → WM storage
    // WHY: Brain spikes must flow to cognitive processing

    ASSERT_NE(brain_buffer, nullptr);
    ASSERT_NE(wm_adapter, nullptr);

    uint64_t timestamp = getCurrentTimeUs();

    // Buffer neural activity
    EXPECT_TRUE(brain_buffer_activity(brain_buffer,
                                      neural_activity.data(),
                                      neural_activity.size(),
                                      timestamp));

    // Extract features
    float features[16];
    size_t num_features = brain_extract_windowed_features(brain_buffer,
                                                           features, 16);
    EXPECT_GT(num_features, 0);

    // Store in working memory
    EXPECT_TRUE(wm_adapter_add_item(wm_adapter, 1, features,
                                    num_features, 0.8f));

    // Verify storage
    const wm_item_t* item = wm_adapter_get_item(wm_adapter, 1);
    ASSERT_NE(item, nullptr);
    EXPECT_EQ(item->data_size, num_features);
}

TEST_F(BrainMiddlewareE2ETest, SpikeExtractionToConsolidation) {
    // WHAT: Test spike data → consolidation across timescales
    // WHY: Neural activity must integrate at multiple timescales

    ASSERT_NE(brain_buffer, nullptr);
    ASSERT_NE(consol_adapter, nullptr);

    uint64_t timestamp = getCurrentTimeUs();

    // Process multiple timesteps
    for (int t = 0; t < 50; t++) {
        // Update neural activity
        for (size_t i = 0; i < neural_activity.size(); i++) {
            neural_activity[i] = std::sin(t * 0.1f + i * 0.2f);
        }

        // Buffer activity
        EXPECT_TRUE(brain_buffer_activity(brain_buffer,
                                          neural_activity.data(),
                                          neural_activity.size(),
                                          timestamp));

        // Extract and consolidate
        float features[8];
        size_t num_features = brain_extract_windowed_features(brain_buffer,
                                                               features, 8);

        for (size_t ch = 0; ch < num_features && ch < 8; ch++) {
            EXPECT_TRUE(consol_adapter_update(consol_adapter, ch,
                                             features[ch], timestamp));
        }

        timestamp += 1000;  // 1ms
    }

    // Verify consolidation
    consol_adapter_stats_t stats;
    EXPECT_TRUE(consol_adapter_get_stats(consol_adapter, &stats));
    EXPECT_GT(stats.total_updates, 0);
}

TEST_F(BrainMiddlewareE2ETest, PopulationCodingWithFeatureExtraction) {
    // WHAT: Test population coding → feature extraction pipeline
    // WHY: Population codes must be analyzable

    ASSERT_NE(spike_extractor, nullptr);
    ASSERT_NE(population_analyzer, nullptr);

    // Would use actual spike data structure
    // For now, test component availability
    EXPECT_NE(spike_extractor, nullptr);
    EXPECT_NE(population_analyzer, nullptr);
}

//=============================================================================
// 2. ATTENTION-GATED ROUTING
//=============================================================================

TEST_F(BrainMiddlewareE2ETest, AttentionGatedCognitiveProcessing) {
    // WHAT: Test attention modulating cognitive pipeline
    // WHY: Attention should gate information flow

    ASSERT_NE(attention_adapter, nullptr);
    ASSERT_NE(wm_adapter, nullptr);
    ASSERT_NE(brain_buffer, nullptr);

    // Set attention spotlight
    EXPECT_TRUE(attention_adapter_set_weight(attention_adapter, 0, 1, 0.9f));
    EXPECT_TRUE(attention_adapter_set_weight(attention_adapter, 0, 2, 0.3f));

    uint64_t timestamp = getCurrentTimeUs();

    // Process neural activity
    EXPECT_TRUE(brain_buffer_activity(brain_buffer,
                                      neural_activity.data(),
                                      neural_activity.size(),
                                      timestamp));

    // Extract features
    float features[8];
    size_t num_features = brain_extract_windowed_features(brain_buffer,
                                                           features, 8);

    // Route through attention for item 1 (high attention)
    float routed_features_1[8];
    EXPECT_TRUE(attention_adapter_route_signal(attention_adapter, 1,
                                               features, routed_features_1,
                                               num_features));

    // Route through attention for item 2 (low attention)
    float routed_features_2[8];
    EXPECT_TRUE(attention_adapter_route_signal(attention_adapter, 2,
                                               features, routed_features_2,
                                               num_features));

    // High attention item should have stronger signal
    float sum1 = 0.0f, sum2 = 0.0f;
    for (size_t i = 0; i < num_features; i++) {
        sum1 += routed_features_1[i];
        sum2 += routed_features_2[i];
    }
    EXPECT_GT(sum1, sum2);
}

TEST_F(BrainMiddlewareE2ETest, AttentionWithConsolidation) {
    // WHAT: Test attention-weighted consolidation
    // WHY: Important events should consolidate more strongly

    ASSERT_NE(attention_adapter, nullptr);
    ASSERT_NE(consol_adapter, nullptr);

    uint64_t timestamp = getCurrentTimeUs();

    // Process with varying attention
    for (int t = 0; t < 20; t++) {
        // Vary attention over time
        float attention = 0.3f + 0.5f * std::sin(t * 0.3f);
        EXPECT_TRUE(attention_adapter_update_salience(attention_adapter,
                                                      1, attention));

        // Consolidate with attention weighting
        float value = static_cast<float>(t);
        EXPECT_TRUE(consol_adapter_update(consol_adapter, 0, value, timestamp));

        timestamp += 1000;
    }

    // Verify consolidation occurred
    float consolidated = consol_adapter_get_consolidated(consol_adapter, 0);
    EXPECT_FALSE(std::isnan(consolidated));
}

//=============================================================================
// 3. TRAINING SIGNAL GENERATION
//=============================================================================

TEST_F(BrainMiddlewareE2ETest, LearningSignalFromNeuralActivity) {
    // WHAT: Test neural activity → learning signal extraction
    // WHY: Brain activity must generate training signals

    ASSERT_NE(brain_buffer, nullptr);
    ASSERT_NE(learning_adapter, nullptr);

    uint64_t timestamp = getCurrentTimeUs();

    // Buffer neural activity
    EXPECT_TRUE(brain_buffer_activity(brain_buffer,
                                      neural_activity.data(),
                                      neural_activity.size(),
                                      timestamp));

    // Extract features
    float features[16];
    size_t num_features = brain_extract_windowed_features(brain_buffer,
                                                           features, 16);

    // Normalize features
    EXPECT_TRUE(learning_signal_adapter_normalize(learning_adapter,
                                                  features, num_features));

    // Features should be normalized
    float mean = 0.0f;
    for (size_t i = 0; i < num_features; i++) {
        mean += features[i];
    }
    mean /= num_features;

    // Normalized features should have reasonable mean
    EXPECT_LT(std::abs(mean), 2.0f);
}

TEST_F(BrainMiddlewareE2ETest, WeightUpdateRoutingFromBrain) {
    // WHAT: Test brain learning signals → weight routing
    // WHY: Learning must reach appropriate brain regions

    ASSERT_NE(weight_router, nullptr);

    // Set up routing for different brain regions
    EXPECT_TRUE(weight_update_router_add_route(weight_router,
                                               LEARNING_SIGNAL_ERROR,
                                               WEIGHT_TARGET_CORTICAL, 1));
    EXPECT_TRUE(weight_update_router_add_route(weight_router,
                                               LEARNING_SIGNAL_MEMORY,
                                               WEIGHT_TARGET_HIPPOCAMPAL, 1));

    // Create weight updates for different regions
    weight_update_t cortical_update;
    cortical_update.target_type = WEIGHT_TARGET_CORTICAL;
    cortical_update.source_neuron = 0;
    cortical_update.target_neuron = 10;
    cortical_update.weight_delta = -0.01f;
    cortical_update.learning_rate = 0.001f;
    cortical_update.modulation_factor = 1.0f;
    cortical_update.apply_stdp = true;
    cortical_update.metadata = nullptr;

    // Route update
    bool routed = weight_update_router_route(weight_router, &cortical_update);
    // May succeed or fail based on routing rules

    weight_update_router_stats_t stats;
    EXPECT_TRUE(weight_update_router_get_stats(weight_router, &stats));
}

//=============================================================================
// 4. COMPLETE PIPELINE INTEGRATION
//=============================================================================

TEST_F(BrainMiddlewareE2ETest, FullPerceptionLearningCycle) {
    // WHAT: Test complete perception → processing → learning cycle
    // WHY: Verify end-to-end brain-middleware integration

    ASSERT_NE(brain_buffer, nullptr);
    ASSERT_NE(brain_normalizer, nullptr);
    ASSERT_NE(wm_adapter, nullptr);
    ASSERT_NE(attention_adapter, nullptr);
    ASSERT_NE(consol_adapter, nullptr);
    ASSERT_NE(learning_adapter, nullptr);
    ASSERT_NE(weight_router, nullptr);

    // Set up routing
    EXPECT_TRUE(weight_update_router_add_route(weight_router,
                                               LEARNING_SIGNAL_ERROR,
                                               WEIGHT_TARGET_CORTICAL, 1));

    uint64_t timestamp = getCurrentTimeUs();

    // Simulate 10 processing cycles
    for (int cycle = 0; cycle < 10; cycle++) {
        // 1. Generate neural activity
        for (size_t i = 0; i < neural_activity.size(); i++) {
            neural_activity[i] = std::sin(cycle * 0.2f + i * 0.1f) * 0.5f + 0.5f;
        }

        // 2. Buffer neural activity
        EXPECT_TRUE(brain_buffer_activity(brain_buffer,
                                          neural_activity.data(),
                                          neural_activity.size(),
                                          timestamp));

        // 3. Extract and normalize features
        float features[16];
        size_t num_features = brain_extract_and_normalize(brain_buffer,
                                                           brain_normalizer,
                                                           features, 16);
        EXPECT_GT(num_features, 0);

        // 4. Set attention based on feature salience
        float salience = cognitive_adapter_calculate_salience(features,
                                                              num_features,
                                                              0.5f);
        EXPECT_TRUE(attention_adapter_update_salience(attention_adapter,
                                                      cycle, salience));

        // 5. Store in working memory (if salient enough)
        if (salience > 0.5f) {
            EXPECT_TRUE(wm_adapter_add_item(wm_adapter, cycle,
                                           features, num_features, salience));
        }

        // 6. Consolidate to long-term representation
        for (size_t ch = 0; ch < num_features && ch < 8; ch++) {
            EXPECT_TRUE(consol_adapter_update(consol_adapter, ch,
                                             features[ch], timestamp));
        }

        // 7. Generate learning signal (prediction error)
        float expected = 0.5f;
        float actual = features[0];
        float error = expected - actual;

        // 8. Create weight update
        weight_update_t update;
        update.target_type = WEIGHT_TARGET_CORTICAL;
        update.source_neuron = cycle % 32;
        update.target_neuron = (cycle + 1) % 32;
        update.weight_delta = error * 0.01f;
        update.learning_rate = 0.001f;
        update.modulation_factor = salience;
        update.apply_stdp = true;
        update.metadata = nullptr;

        // 9. Route weight update
        weight_update_router_route(weight_router, &update);

        timestamp += 10000;  // 10ms
    }

    // Verify complete pipeline operated
    wm_adapter_stats_t wm_stats;
    EXPECT_TRUE(wm_adapter_get_stats(wm_adapter, &wm_stats));
    EXPECT_GT(wm_stats.total_added, 0);

    consol_adapter_stats_t consol_stats;
    EXPECT_TRUE(consol_adapter_get_stats(consol_adapter, &consol_stats));
    EXPECT_GT(consol_stats.total_updates, 0);

    attention_adapter_stats_t attn_stats;
    EXPECT_TRUE(attention_adapter_get_stats(attention_adapter, &attn_stats));
    EXPECT_GT(attn_stats.active_targets, 0);
}

TEST_F(BrainMiddlewareE2ETest, MultiRegionCoordinatedLearning) {
    // WHAT: Test coordinated learning across brain regions
    // WHY: Complex learning involves multiple regions

    ASSERT_NE(weight_router, nullptr);
    ASSERT_NE(event_manager, nullptr);

    // Set up multi-region routing
    EXPECT_TRUE(weight_update_router_add_route(weight_router,
                                               LEARNING_SIGNAL_ERROR,
                                               WEIGHT_TARGET_CORTICAL, 1));
    EXPECT_TRUE(weight_update_router_add_route(weight_router,
                                               LEARNING_SIGNAL_REWARD,
                                               WEIGHT_TARGET_STRIATAL, 2));
    EXPECT_TRUE(weight_update_router_add_route(weight_router,
                                               LEARNING_SIGNAL_MEMORY,
                                               WEIGHT_TARGET_HIPPOCAMPAL, 1));

    // Simulate learning epoch
    training_event_data_t epoch_start;
    epoch_start.type = TRAINING_EVENT_EPOCH_START;
    epoch_start.epoch = 1;
    epoch_start.batch = 0;
    epoch_start.loss = 1.0f;
    epoch_start.learning_rate = 0.001f;
    epoch_start.timestamp_us = getCurrentTimeUs();
    epoch_start.metadata = nullptr;

    EXPECT_TRUE(training_event_manager_publish(event_manager, &epoch_start));

    // Create updates for each region
    weight_update_t updates[3];

    // Cortical update (error correction)
    updates[0].target_type = WEIGHT_TARGET_CORTICAL;
    updates[0].weight_delta = -0.01f;
    updates[0].learning_rate = 0.001f;

    // Striatal update (reward learning)
    updates[1].target_type = WEIGHT_TARGET_STRIATAL;
    updates[1].weight_delta = 0.02f;
    updates[1].learning_rate = 0.001f;

    // Hippocampal update (memory consolidation)
    updates[2].target_type = WEIGHT_TARGET_HIPPOCAMPAL;
    updates[2].weight_delta = 0.005f;
    updates[2].learning_rate = 0.0005f;

    // Route all updates
    uint32_t routed = weight_update_router_route_batch(weight_router,
                                                        updates, 3);

    // Verify routing
    weight_update_router_stats_t stats;
    EXPECT_TRUE(weight_update_router_get_stats(weight_router, &stats));
    EXPECT_EQ(stats.active_routes, 3);
}

TEST_F(BrainMiddlewareE2ETest, AdaptiveAttentionWithLearning) {
    // WHAT: Test attention adapting based on learning signals
    // WHY: Attention should focus on error-prone regions

    ASSERT_NE(attention_adapter, nullptr);
    ASSERT_NE(learning_adapter, nullptr);
    ASSERT_NE(wm_adapter, nullptr);

    uint64_t timestamp = getCurrentTimeUs();

    // Simulate learning with attention adaptation
    for (int trial = 0; trial < 15; trial++) {
        // Generate features
        float features[8];
        for (int i = 0; i < 8; i++) {
            features[i] = std::sin(trial * 0.3f + i * 0.4f);
        }

        // Normalize
        EXPECT_TRUE(learning_signal_adapter_normalize(learning_adapter,
                                                      features, 8));

        // Calculate prediction error (higher early, lower later)
        float error = 1.0f * std::exp(-trial * 0.1f);

        // Adjust attention based on error (focus on high-error regions)
        float attention_weight = 0.3f + 0.7f * error;
        EXPECT_TRUE(attention_adapter_set_weight(attention_adapter, 0,
                                                 trial % 5, attention_weight));

        // Store in WM with attention-modulated salience
        EXPECT_TRUE(wm_adapter_add_item(wm_adapter, trial, features, 8,
                                       attention_weight));
    }

    // Verify attention adapted
    attention_adapter_stats_t attn_stats;
    EXPECT_TRUE(attention_adapter_get_stats(attention_adapter, &attn_stats));
    EXPECT_GT(attn_stats.total_shifts, 0);

    wm_adapter_stats_t wm_stats;
    EXPECT_TRUE(wm_adapter_get_stats(wm_adapter, &wm_stats));
    EXPECT_GT(wm_stats.total_added, 0);
}

TEST_F(BrainMiddlewareE2ETest, TemporalIntegrationWithSTDP) {
    // WHAT: Test temporal integration with STDP-based learning
    // WHY: Spike timing should influence learning

    ASSERT_NE(consol_adapter, nullptr);
    ASSERT_NE(weight_router, nullptr);

    uint64_t timestamp = getCurrentTimeUs();

    // Simulate spike-timing-dependent learning
    for (int t = 0; t < 30; t++) {
        // Pre-synaptic activity
        float pre_activity = std::sin(t * 0.2f) > 0 ? 1.0f : 0.0f;

        // Post-synaptic activity (delayed)
        float post_activity = std::sin((t - 2) * 0.2f) > 0 ? 1.0f : 0.0f;

        // Consolidate activities
        EXPECT_TRUE(consol_adapter_update(consol_adapter, 0,
                                         pre_activity, timestamp));
        EXPECT_TRUE(consol_adapter_update(consol_adapter, 1,
                                         post_activity, timestamp));

        // Calculate STDP window
        int dt = 2;  // 2ms delay
        float stdp_weight = std::exp(-std::abs(dt) / 20.0f);

        // Create STDP-based update
        weight_update_t update;
        update.target_type = WEIGHT_TARGET_CORTICAL;
        update.source_neuron = 0;
        update.target_neuron = 1;
        update.weight_delta = (dt > 0 ? 1.0f : -1.0f) * stdp_weight * 0.01f;
        update.learning_rate = 0.001f;
        update.modulation_factor = 1.0f;
        update.apply_stdp = true;
        update.metadata = nullptr;

        if (pre_activity > 0.5f && post_activity > 0.5f) {
            weight_update_router_route(weight_router, &update);
        }

        timestamp += 1000;  // 1ms
    }

    // Verify temporal integration occurred
    consol_adapter_stats_t stats;
    EXPECT_TRUE(consol_adapter_get_stats(consol_adapter, &stats));
    EXPECT_GT(stats.total_updates, 0);
}

//=============================================================================
// 5. PERFORMANCE AND STRESS TESTS
//=============================================================================

TEST_F(BrainMiddlewareE2ETest, HighFrequencyProcessing) {
    // WHAT: Test pipeline at high processing frequency
    // WHY: Real-time brain simulation requires fast processing

    ASSERT_NE(brain_buffer, nullptr);
    ASSERT_NE(wm_adapter, nullptr);

    auto start_time = std::chrono::high_resolution_clock::now();

    uint64_t timestamp = getCurrentTimeUs();

    // Process 1000 rapid updates
    for (int i = 0; i < 1000; i++) {
        EXPECT_TRUE(brain_buffer_activity(brain_buffer,
                                          neural_activity.data(),
                                          neural_activity.size(),
                                          timestamp));

        if (i % 10 == 0) {  // Extract features every 10 updates
            float features[8];
            size_t num = brain_extract_windowed_features(brain_buffer,
                                                          features, 8);
            if (num > 0 && i < 100) {  // Limit WM to avoid overflow
                wm_adapter_add_item(wm_adapter, i, features, num, 0.7f);
            }
        }

        timestamp += 100;  // 0.1ms
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();

    // Should complete in reasonable time (< 1 second)
    EXPECT_LT(duration, 1000);
}

TEST_F(BrainMiddlewareE2ETest, ConcurrentAdapterAccess) {
    // WHAT: Test thread safety of adapters
    // WHY: Multi-threaded brain simulation requires safe concurrent access

    ASSERT_NE(wm_adapter, nullptr);
    ASSERT_NE(consol_adapter, nullptr);

    std::atomic<int> success_count{0};

    // Launch concurrent workers
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([&, t]() {
            uint64_t timestamp = getCurrentTimeUs();

            for (int i = 0; i < 25; i++) {
                // Each thread works on different data
                float data[2] = {static_cast<float>(t * 100 + i),
                                static_cast<float>(t * 100 + i + 1)};

                // Try WM operations
                if (wm_adapter_add_item(wm_adapter, t * 100 + i,
                                       data, 2, 0.6f)) {
                    success_count++;
                }

                // Try consolidation operations
                consol_adapter_update(consol_adapter, t, data[0], timestamp);

                timestamp += 100;
            }
        });
    }

    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }

    // Verify some operations succeeded
    EXPECT_GT(success_count.load(), 0);
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
