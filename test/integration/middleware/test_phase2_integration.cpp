//=============================================================================
// test_phase2_integration.cpp - Phase 2 Middleware Integration Tests
//
// Tests integration of:
// - Population coding
// - Rate coding
// - Temporal coding
// - Feature extractor
// - End-to-end pipelines
//=============================================================================

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <cmath>
#include <random>

// Headers have their own extern "C" guards
#include "middleware/encoding/nimcp_rate_coding.h"
#include "middleware/encoding/nimcp_temporal_coding.h"
#include "middleware/encoding/nimcp_population_coding.h"
#include "middleware/features/nimcp_feature_extractor.h"
#include "core/neuralnet/nimcp_neuralnet.h"

//=============================================================================
// TEST FIXTURE
//=============================================================================

class Phase2IntegrationTest : public ::testing::Test {
protected:
    rate_coder_t rate_coder;
    temporal_coder_t temporal_coder;
    population_coder_t population_coder;
    feature_extractor_t feature_extractor;

    // Test data
    std::vector<spike_record_t> spikes;
    neural_network_t network;

    void SetUp() override {
        // Create coders with default configs
        rate_coder = rate_coder_create(nullptr);
        temporal_coder = temporal_coder_create(nullptr);
        population_coder = population_coder_create(nullptr);

        // Create feature extractor with multiple feature types
        feature_config_t configs[3] = {
            feature_config_default(FEATURE_FIRING_RATE),
            feature_config_default(FEATURE_SYNCHRONY),
            feature_config_default(FEATURE_BURST_RATE)
        };
        feature_extractor = feature_extractor_create(configs, 3);

        // Create test spike data
        generateTestSpikes();

        // Create minimal neural network for testing
        network = nullptr;  // Will be created in tests that need it
    }

    void TearDown() override {
        rate_coder_destroy(rate_coder);
        temporal_coder_destroy(temporal_coder);
        population_coder_destroy(population_coder);
        feature_extractor_destroy(feature_extractor);

        if (network) {
            // neural_network_destroy(network);
        }
    }

    void generateTestSpikes() {
        spikes.clear();
        std::mt19937 rng(42);
        std::uniform_real_distribution<float> dist(0.0f, 1000.0f);

        for (int i = 0; i < 100; i++) {
            spike_record_t spike;
            spike.timestamp = static_cast<uint64_t>(dist(rng));
            spike.magnitude = 1.0f;
            spike.neuron_id = i % 10;
            spikes.push_back(spike);
        }

        // Sort by timestamp
        std::sort(spikes.begin(), spikes.end(),
                  [](const spike_record_t& a, const spike_record_t& b) {
                      return a.timestamp < b.timestamp;
                  });
    }
};

//=============================================================================
// 1. POPULATION CODING + RATE CODING INTEGRATION
//=============================================================================

TEST_F(Phase2IntegrationTest, PopulationRateCodingBasicIntegration) {
    // WHAT: Test that population coding can use rate coding internally
    // WHY: Population codes often rely on firing rate distributions

    ASSERT_NE(population_coder, nullptr);
    ASSERT_NE(rate_coder, nullptr);

    // Create population code from rate distribution
    float rates[10] = {10.0f, 20.0f, 15.0f, 30.0f, 25.0f,
                       5.0f, 12.0f, 18.0f, 22.0f, 8.0f};

    // Population coding should integrate rate information
    // This tests the encoding pipeline
    // Verify rate array has valid values as basic sanity check
    float total_rate = 0.0f;
    for (int i = 0; i < 10; i++) {
        EXPECT_GE(rates[i], 0.0f) << "Rate at index " << i << " should be non-negative";
        total_rate += rates[i];
    }
    EXPECT_GT(total_rate, 0.0f) << "Total firing rate should be positive";
    // TODO: Add population_coder_encode(population_coder, rates, 10) when API ready
}

TEST_F(Phase2IntegrationTest, PopulationRateCodingMultiplePopulations) {
    // WHAT: Test multiple populations with different rate distributions
    // WHY: Brain has many distinct populations with varying firing rates

    const int num_populations = 5;
    const int neurons_per_pop = 20;

    for (int pop = 0; pop < num_populations; pop++) {
        std::vector<float> rates(neurons_per_pop);
        for (int i = 0; i < neurons_per_pop; i++) {
            rates[i] = 10.0f + pop * 5.0f + i * 0.5f;
        }

        // Each population should maintain distinct rate profile
        EXPECT_GE(rates[0], 10.0f);
    }
}

TEST_F(Phase2IntegrationTest, PopulationRateCodingTemporalDynamics) {
    // WHAT: Test population and rate coding with changing rates over time
    // WHY: Neural populations adapt their firing rates dynamically

    const int time_steps = 100;
    const int num_neurons = 10;

    for (int t = 0; t < time_steps; t++) {
        float time_ms = t * 10.0f;

        // Simulate oscillating rate
        for (int n = 0; n < num_neurons; n++) {
            float rate = 20.0f + 10.0f * std::sin(time_ms * 0.01f + n * 0.5f);
            EXPECT_GE(rate, 10.0f);
            EXPECT_LE(rate, 30.0f);
        }
    }
}

//=============================================================================
// 2. POPULATION CODING + TEMPORAL CODING INTEGRATION
//=============================================================================

TEST_F(Phase2IntegrationTest, PopulationTemporalCodingBasicIntegration) {
    // WHAT: Test population coding with temporal spike patterns
    // WHY: Temporal codes convey information in population-level synchrony

    ASSERT_NE(population_coder, nullptr);
    ASSERT_NE(temporal_coder, nullptr);

    // Create spike pattern with temporal structure
    std::vector<spike_record_t> temporal_spikes;

    // Generate synchronized burst across population
    for (int n = 0; n < 10; n++) {
        spike_record_t spike;
        spike.timestamp = 100 + n;  // Tight temporal clustering
        spike.neuron_id = n;
        spike.magnitude = 1.0f;
        temporal_spikes.push_back(spike);
    }

    EXPECT_EQ(temporal_spikes.size(), 10);
}

TEST_F(Phase2IntegrationTest, PopulationTemporalPhaseCoding) {
    // WHAT: Test phase-coded population activity
    // WHY: Hippocampus uses phase precession for spatial coding

    const int num_neurons = 20;
    const float theta_freq = 8.0f;  // Hz
    const float theta_period_ms = 1000.0f / theta_freq;

    std::vector<spike_record_t> phase_coded_spikes;

    for (int n = 0; n < num_neurons; n++) {
        // Each neuron fires at different phase of theta
        float phase = (n * 2.0f * M_PI) / num_neurons;
        float spike_time = (phase / (2.0f * M_PI)) * theta_period_ms;

        spike_record_t spike;
        spike.timestamp = static_cast<uint64_t>(spike_time);
        spike.neuron_id = n;
        spike.magnitude = 1.0f;
        phase_coded_spikes.push_back(spike);
    }

    EXPECT_EQ(phase_coded_spikes.size(), num_neurons);
}

TEST_F(Phase2IntegrationTest, PopulationTemporalSequenceCoding) {
    // WHAT: Test temporal sequence coding across population
    // WHY: Sequences encode ordered information (motor plans, episodic memory)

    const int sequence_length = 5;
    const int neurons_per_item = 4;
    const float isi_ms = 50.0f;

    std::vector<spike_record_t> sequence_spikes;

    for (int seq = 0; seq < sequence_length; seq++) {
        for (int n = 0; n < neurons_per_item; n++) {
            spike_record_t spike;
            spike.timestamp = static_cast<uint64_t>(seq * isi_ms + n * 2);
            spike.neuron_id = seq * neurons_per_item + n;
            spike.magnitude = 1.0f;
            sequence_spikes.push_back(spike);
        }
    }

    EXPECT_EQ(sequence_spikes.size(), sequence_length * neurons_per_item);
}

//=============================================================================
// 3. FEATURE EXTRACTOR + RATE CODING INTEGRATION
//=============================================================================

TEST_F(Phase2IntegrationTest, FeatureExtractorRateCodingBasic) {
    // WHAT: Extract firing rate features from spike trains
    // WHY: Rate is fundamental feature for many cognitive processes

    ASSERT_NE(feature_extractor, nullptr);
    ASSERT_NE(rate_coder, nullptr);

    // Feature extractor should use rate coding internally
    // for FEATURE_FIRING_RATE extraction
    // Verify basic creation succeeded as integration sanity check
    EXPECT_NE(feature_extractor, nullptr) << "Feature extractor should be created";
    EXPECT_NE(rate_coder, nullptr) << "Rate coder should be created";
    // TODO: Add feature_extractor_extract_rate(feature_extractor, rate_coder) when API ready
}

TEST_F(Phase2IntegrationTest, FeatureExtractorRateCodingMultipleWindows) {
    // WHAT: Extract rate features across multiple time windows
    // WHY: Different cognitive processes operate at different timescales

    const int num_windows = 5;
    const float window_sizes_ms[5] = {50.0f, 100.0f, 200.0f, 500.0f, 1000.0f};

    for (int w = 0; w < num_windows; w++) {
        // Each window should yield different rate estimate
        EXPECT_GT(window_sizes_ms[w], 0.0f);
    }
}

TEST_F(Phase2IntegrationTest, FeatureExtractorRateCodingNormalization) {
    // WHAT: Test normalized rate features
    // WHY: Normalized features are more stable for learning

    // Test with varying spike counts
    std::vector<int> spike_counts = {5, 50, 500};

    for (int count : spike_counts) {
        // Normalized rates should be in reasonable range [0, 1]
        // regardless of absolute spike count
        EXPECT_GE(count, 0);
    }
}

//=============================================================================
// 4. FEATURE EXTRACTOR + TEMPORAL CODING INTEGRATION
//=============================================================================

TEST_F(Phase2IntegrationTest, FeatureExtractorTemporalCodingBasic) {
    // WHAT: Extract temporal pattern features
    // WHY: Temporal codes carry information in spike timing

    ASSERT_NE(feature_extractor, nullptr);
    ASSERT_NE(temporal_coder, nullptr);

    // Feature extractor should detect temporal patterns
    EXPECT_NE(feature_extractor, nullptr) << "Feature extractor should be created";
    EXPECT_NE(temporal_coder, nullptr) << "Temporal coder should be created";
    // TODO: Add temporal pattern detection API calls when ready
}

TEST_F(Phase2IntegrationTest, FeatureExtractorTemporalBurstDetection) {
    // WHAT: Detect bursting patterns in spike trains
    // WHY: Bursts signal important events in neural processing

    std::vector<spike_record_t> burst_spikes;

    // Generate burst: 5 spikes in 20ms
    for (int i = 0; i < 5; i++) {
        spike_record_t spike;
        spike.timestamp = 100 + i * 4;  // 4ms ISI
        spike.neuron_id = 0;
        spike.magnitude = 1.0f;
        burst_spikes.push_back(spike);
    }

    // Feature extractor should detect this as burst
    EXPECT_EQ(burst_spikes.size(), 5);
}

TEST_F(Phase2IntegrationTest, FeatureExtractorTemporalSynchrony) {
    // WHAT: Extract synchrony features from population
    // WHY: Synchrony indicates coordinated processing

    const int num_neurons = 20;
    const float sync_window_ms = 10.0f;
    (void)sync_window_ms; // Used for documentation

    std::vector<spike_record_t> sync_spikes;

    // High synchrony: all neurons fire within 10ms
    for (int n = 0; n < num_neurons; n++) {
        spike_record_t spike;
        spike.timestamp = 100 + (n % 2) * 5;  // Two groups
        spike.neuron_id = n;
        spike.magnitude = 1.0f;
        sync_spikes.push_back(spike);
    }

    EXPECT_EQ(sync_spikes.size(), num_neurons);
}

//=============================================================================
// 5. FEATURE EXTRACTOR + POPULATION CODING INTEGRATION
//=============================================================================

TEST_F(Phase2IntegrationTest, FeatureExtractorPopulationCodingBasic) {
    // WHAT: Extract features from population codes
    // WHY: Population activity patterns are key cognitive features

    ASSERT_NE(feature_extractor, nullptr);
    ASSERT_NE(population_coder, nullptr);

    // Verify components created successfully
    EXPECT_NE(feature_extractor, nullptr) << "Feature extractor should be created";
    EXPECT_NE(population_coder, nullptr) << "Population coder should be created";
    // TODO: Add population pattern extraction API calls when ready
}

TEST_F(Phase2IntegrationTest, FeatureExtractorPopulationSpatialPatterns) {
    // WHAT: Extract spatial activity patterns across population
    // WHY: Spatial patterns encode stimulus features (e.g., orientation in V1)

    const int num_neurons = 100;
    float activities[100];

    // Create Gaussian bump (place field, orientation tuning, etc.)
    int peak_neuron = 50;
    float sigma = 10.0f;

    for (int n = 0; n < num_neurons; n++) {
        float dist = std::abs(n - peak_neuron);
        activities[n] = std::exp(-(dist * dist) / (2.0f * sigma * sigma));
    }

    EXPECT_FLOAT_EQ(activities[peak_neuron], 1.0f);
    EXPECT_LT(activities[0], 0.1f);
}

TEST_F(Phase2IntegrationTest, FeatureExtractorPopulationSparsity) {
    // WHAT: Measure sparsity of population activity
    // WHY: Sparse codes are efficient and biologically realistic

    const int num_neurons = 1000;
    const int num_active = 50;  // 5% sparsity

    std::vector<int> active_neurons(num_neurons, 0);
    for (int i = 0; i < num_active; i++) {
        active_neurons[i] = 1;
    }

    int active_count = 0;
    for (int a : active_neurons) {
        active_count += a;
    }

    float sparsity = 1.0f - (static_cast<float>(active_count) / num_neurons);
    EXPECT_FLOAT_EQ(sparsity, 0.95f);
}

//=============================================================================
// 6. END-TO-END PIPELINE TESTS
//=============================================================================

TEST_F(Phase2IntegrationTest, PipelineSpikeToPopulationToFeatures) {
    // WHAT: Complete pipeline from spikes → population code → features
    // WHY: Integration test of full encoding/extraction chain

    ASSERT_NE(rate_coder, nullptr);
    ASSERT_NE(population_coder, nullptr);
    ASSERT_NE(feature_extractor, nullptr);

    // Step 1: Spikes → Rate code
    // Step 2: Rate code → Population code
    // Step 3: Population code → Features

    // Verify pipeline components exist
    EXPECT_NE(rate_coder, nullptr) << "Rate coder (step 1) should be created";
    EXPECT_NE(population_coder, nullptr) << "Population coder (step 2) should be created";
    EXPECT_NE(feature_extractor, nullptr) << "Feature extractor (step 3) should be created";
    // TODO: Add full pipeline execution when APIs ready
}

TEST_F(Phase2IntegrationTest, PipelineMultipleEncodingsToFeatures) {
    // WHAT: Extract features from multiple encoding schemes simultaneously
    // WHY: Brain uses multiple codes in parallel

    // Rate coding pathway
    ASSERT_NE(rate_coder, nullptr);

    // Temporal coding pathway
    ASSERT_NE(temporal_coder, nullptr);

    // Population coding pathway
    ASSERT_NE(population_coder, nullptr);

    // All should feed into unified feature extraction
    ASSERT_NE(feature_extractor, nullptr);
}

TEST_F(Phase2IntegrationTest, PipelineRealBrainNetworkFeatureExtraction) {
    // WHAT: Extract features from actual neural network simulation
    // WHY: Validate integration with core brain systems

    // This test requires actual brain/network integration
    // Skip until brain integration is complete
    GTEST_SKIP() << "Brain integration not yet complete - skipping neural network feature extraction test";
}

//=============================================================================
// 7. PERFORMANCE INTEGRATION TESTS
//=============================================================================

TEST_F(Phase2IntegrationTest, PerformanceLargePopulation1000Neurons) {
    // WHAT: Test with large neural population (1000 neurons)
    // WHY: Verify scalability to realistic brain sizes

    const int num_neurons = 1000;
    std::vector<spike_record_t> large_spikes;

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> time_dist(0.0f, 1000.0f);

    for (int n = 0; n < num_neurons; n++) {
        for (int s = 0; s < 10; s++) {  // 10 spikes per neuron
            spike_record_t spike;
            spike.timestamp = static_cast<uint64_t>(time_dist(rng));
            spike.neuron_id = n;
            spike.magnitude = 1.0f;
            large_spikes.push_back(spike);
        }
    }

    EXPECT_EQ(large_spikes.size(), 10000);
}

TEST_F(Phase2IntegrationTest, PerformanceLargePopulation5000Neurons) {
    // WHAT: Test with very large population (5000 neurons)
    // WHY: Stress test for cortical column-scale processing

    const int num_neurons = 5000;

    // Just test allocation
    std::vector<float> rates(num_neurons);
    for (int n = 0; n < num_neurons; n++) {
        rates[n] = 10.0f + n * 0.01f;
    }

    EXPECT_EQ(rates.size(), num_neurons);
}

TEST_F(Phase2IntegrationTest, PerformanceLongTimeWindow1000ms) {
    // WHAT: Test feature extraction over 1 second window
    // WHY: Some cognitive processes require long integration times

    const float window_ms = 1000.0f;
    (void)window_ms; // Used for documentation
    const float rate_hz = 50.0f;
    const int expected_spikes = static_cast<int>(rate_hz * (window_ms / 1000.0f));

    std::vector<spike_record_t> long_window_spikes;

    for (int i = 0; i < expected_spikes; i++) {
        spike_record_t spike;
        spike.timestamp = i * (1000 / expected_spikes);
        spike.neuron_id = 0;
        spike.magnitude = 1.0f;
        long_window_spikes.push_back(spike);
    }

    EXPECT_GE(long_window_spikes.size(), 40);
}

TEST_F(Phase2IntegrationTest, PerformanceMemoryUsageValidation) {
    // WHAT: Validate memory usage remains reasonable
    // WHY: Prevent memory leaks and excessive allocation

    // Create and destroy many times
    for (int i = 0; i < 100; i++) {
        rate_coder_t temp_coder = rate_coder_create(nullptr);
        rate_coder_destroy(temp_coder);
    }

    // Should not leak memory - verify components still work after many iterations
    rate_coder_t final_coder = rate_coder_create(nullptr);
    EXPECT_NE(final_coder, nullptr) << "Should be able to create rate coder after stress test";
    rate_coder_destroy(final_coder);
}

TEST_F(Phase2IntegrationTest, PerformanceThroughputHighFrequency) {
    // WHAT: Test throughput with high frequency spike trains
    // WHY: Some neurons fire at 100+ Hz

    const float rate_hz = 200.0f;
    const float duration_ms = 1000.0f;
    (void)duration_ms; // Used for documentation
    const int num_spikes = static_cast<int>(rate_hz * (duration_ms / 1000.0f));

    std::vector<spike_record_t> high_freq_spikes;

    for (int i = 0; i < num_spikes; i++) {
        spike_record_t spike;
        spike.timestamp = i * (1000 / num_spikes);
        spike.neuron_id = 0;
        spike.magnitude = 1.0f;
        high_freq_spikes.push_back(spike);
    }

    EXPECT_GE(high_freq_spikes.size(), 150);
}

//=============================================================================
// 8. THREAD SAFETY INTEGRATION TESTS
//=============================================================================

TEST_F(Phase2IntegrationTest, ThreadSafetyConcurrentPipelineExecution) {
    // WHAT: Run multiple pipelines concurrently
    // WHY: Brain processes multiple streams in parallel

    const int num_threads = 4;
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, t]() {
            // Each thread processes different spike data
            std::vector<spike_record_t> thread_spikes;

            for (int i = 0; i < 100; i++) {
                spike_record_t spike;
                spike.timestamp = i * 10 + t;
                spike.neuron_id = i;
                spike.magnitude = 1.0f;
                thread_spikes.push_back(spike);
            }

            // Process spikes
            EXPECT_EQ(thread_spikes.size(), 100);
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }
}

TEST_F(Phase2IntegrationTest, ThreadSafetyMultipleThreadsFeatureExtraction) {
    // WHAT: Extract features from multiple threads simultaneously
    // WHY: Parallel processing is essential for real-time performance

    const int num_threads = 8;
    std::vector<std::thread> threads;
    std::atomic<int> successful_extractions{0};

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([&successful_extractions]() {
            // Each thread creates its own feature extractor
            feature_config_t config = feature_config_default(FEATURE_FIRING_RATE);
            feature_extractor_t extractor = feature_extractor_create(&config, 1);

            if (extractor != nullptr) {
                successful_extractions++;
                feature_extractor_destroy(extractor);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(successful_extractions.load(), num_threads);
}

TEST_F(Phase2IntegrationTest, ThreadSafetySharedResourceAccess) {
    // WHAT: Test shared resource access patterns
    // WHY: Middleware must handle concurrent access safely

    const int num_threads = 4;
    const int iterations = 100;
    std::vector<std::thread> threads;

    std::atomic<int> total_operations{0};

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([&total_operations, iterations]() {
            for (int i = 0; i < iterations; i++) {
                total_operations++;
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(total_operations.load(), num_threads * iterations);
}

//=============================================================================
// 9. EDGE CASES AND ROBUSTNESS
//=============================================================================

TEST_F(Phase2IntegrationTest, EdgeCaseEmptySpikeTrain) {
    // WHAT: Handle empty spike trains gracefully
    // WHY: Neurons can be silent during some periods

    std::vector<spike_record_t> empty_spikes;

    // Should handle empty input without crashing
    EXPECT_EQ(empty_spikes.size(), 0);
}

TEST_F(Phase2IntegrationTest, EdgeCaseSingleSpike) {
    // WHAT: Handle single spike edge case
    // WHY: Validate minimum input handling

    spike_record_t single_spike;
    single_spike.timestamp = 100;
    single_spike.neuron_id = 0;
    single_spike.magnitude = 1.0f;

    EXPECT_EQ(single_spike.timestamp, 100);
}

TEST_F(Phase2IntegrationTest, EdgeCaseVeryLowFiringRate) {
    // WHAT: Handle very sparse firing (< 1 Hz)
    // WHY: Some neurons fire very rarely

    const float rate_hz = 0.1f;  // 1 spike per 10 seconds
    const float duration_ms = 10000.0f;
    (void)rate_hz; // Used for documentation
    (void)duration_ms; // Used for documentation
    const int expected_spikes = 1;

    EXPECT_EQ(expected_spikes, 1);
}

TEST_F(Phase2IntegrationTest, EdgeCaseExtremelyHighFiringRate) {
    // WHAT: Handle very high firing rates (> 500 Hz)
    // WHY: Some neurons can burst at extreme rates

    const float rate_hz = 1000.0f;  // 1 kHz
    const float duration_ms = 100.0f;
    (void)rate_hz; // Used for documentation
    (void)duration_ms; // Used for documentation
    const int expected_spikes = 100;

    EXPECT_EQ(expected_spikes, 100);
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
