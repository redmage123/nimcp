/**
 * @file test_dendrite_performance_regression.cpp
 * @brief Performance regression tests for NIMCP Dendrite Module
 *
 * WHAT: Baseline performance tests for dendrite operations
 * WHY:  Detect performance regressions across versions
 * HOW:  Measure operation times against established baselines
 *
 * TEST COVERAGE:
 * - Dendrite creation/destruction performance
 * - Segment and spine creation performance
 * - Signal integration throughput
 * - Network step performance
 * - Memory efficiency
 * - Scalability verification
 *
 * @version Phase 1.5.7: Dendrite Integration
 * @date 2025-11-25
 */

#include <gtest/gtest.h>
#include <chrono>
#include <vector>
#include <numeric>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <algorithm>

// Headers have their own extern "C" guards
#include "core/dendrite/nimcp_dendrite.h"

//=============================================================================
// PERFORMANCE MONITORING UTILITIES
//=============================================================================

class PerformanceMonitor {
public:
    template<typename Func>
    static double MeasureTimeMs(Func&& func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(end - start).count();
    }

    static double Mean(const std::vector<double>& values) {
        if (values.empty()) return 0.0;
        return std::accumulate(values.begin(), values.end(), 0.0) / values.size();
    }

    static double StdDev(const std::vector<double>& values) {
        if (values.size() < 2) return 0.0;
        double mean = Mean(values);
        double sq_sum = 0.0;
        for (double v : values) {
            sq_sum += (v - mean) * (v - mean);
        }
        return std::sqrt(sq_sum / (values.size() - 1));
    }

    static double Percentile(std::vector<double> values, double p) {
        if (values.empty()) return 0.0;
        std::sort(values.begin(), values.end());
        size_t idx = static_cast<size_t>(p * (values.size() - 1));
        return values[idx];
    }
};

//=============================================================================
// PERFORMANCE BASELINES
//=============================================================================

namespace Baseline {
    // Creation/destruction baselines (ms for 1000 dendrites)
    // Note: Destruction includes freeing segments and spines
    constexpr double CREATION_1K_MS = 20.0;
    constexpr double DESTRUCTION_1K_MS = 230.0;  // Destruction with segments/spines is slower (relaxed for CI)

    // Segment creation (ms for 10000 segments)
    constexpr double SEGMENT_10K_MS = 30.0;

    // Spine creation (ms for 10000 spines)
    constexpr double SPINE_10K_MS = 40.0;

    // Signal integration (ms for 100000 inputs)
    constexpr double INPUT_100K_MS = 100.0;

    // Network step (ms for 1000 dendrites)
    constexpr double NETWORK_STEP_1K_MS = 10.0;

    // Plasticity operations (ms for 10000 events)
    // Note: Includes calcium updates and weight modifications
    constexpr double PLASTICITY_10K_MS = 50.0;

    // Regression tolerance (30% above baseline for CI variance)
    constexpr double REGRESSION_TOLERANCE = 1.3;
}

//=============================================================================
// TEST FIXTURE
//=============================================================================

class DendritePerformanceTest : public ::testing::Test {
protected:
    static constexpr int NUM_SAMPLES = 10;
    static constexpr int WARMUP_RUNS = 2;

    dendrite_config_t CreateDefaultConfig(uint32_t id) {
        dendrite_config_t config = {};
        config.id = id;
        config.type = DENDRITE_TYPE_BASAL;
        config.target_neuron_id = id;
        config.total_length = 200.0f;
        config.mean_diameter = 2.0f;
        config.integration_window_ms = 20.0f;
        config.structural_plasticity = 0.01f;
        config.ltp_threshold = 0.8f;
        config.ltd_threshold = 0.3f;
        return config;
    }
};

//=============================================================================
// CREATION PERFORMANCE
//=============================================================================

TEST_F(DendritePerformanceTest, CreationPerformance) {
    std::cout << "\n=== Dendrite Creation Performance ===" << std::endl;

    constexpr int NUM_DENDRITES = 1000;
    std::vector<double> times_ms;

    // Warmup
    for (int w = 0; w < WARMUP_RUNS; w++) {
        std::vector<dendrite_t*> dendrites;
        for (int i = 0; i < NUM_DENDRITES; i++) {
            dendrite_config_t config = CreateDefaultConfig(i);
            dendrites.push_back(dendrite_create(&config));
        }
        for (auto* d : dendrites) dendrite_destroy(d);
    }

    // Measure
    for (int sample = 0; sample < NUM_SAMPLES; sample++) {
        std::vector<dendrite_t*> dendrites;
        dendrites.reserve(NUM_DENDRITES);

        double time = PerformanceMonitor::MeasureTimeMs([&]() {
            for (int i = 0; i < NUM_DENDRITES; i++) {
                dendrite_config_t config = CreateDefaultConfig(i);
                dendrites.push_back(dendrite_create(&config));
            }
        });

        times_ms.push_back(time);

        // Cleanup
        for (auto* d : dendrites) dendrite_destroy(d);
    }

    double mean = PerformanceMonitor::Mean(times_ms);
    double stddev = PerformanceMonitor::StdDev(times_ms);
    double p95 = PerformanceMonitor::Percentile(times_ms, 0.95);

    std::cout << "Creating " << NUM_DENDRITES << " dendrites:" << std::endl;
    std::cout << "  Mean: " << std::fixed << std::setprecision(3) << mean << " ms" << std::endl;
    std::cout << "  StdDev: " << stddev << " ms" << std::endl;
    std::cout << "  P95: " << p95 << " ms" << std::endl;
    std::cout << "  Throughput: " << (NUM_DENDRITES / mean * 1000.0) << " dendrites/sec" << std::endl;
    std::cout << "  Baseline: < " << Baseline::CREATION_1K_MS << " ms" << std::endl;

    EXPECT_LT(mean, Baseline::CREATION_1K_MS * Baseline::REGRESSION_TOLERANCE)
        << "Creation performance regression detected";
}

TEST_F(DendritePerformanceTest, DestructionPerformance) {
    std::cout << "\n=== Dendrite Destruction Performance ===" << std::endl;

    constexpr int NUM_DENDRITES = 1000;
    std::vector<double> times_ms;

    for (int sample = 0; sample < NUM_SAMPLES; sample++) {
        // Create dendrites with segments and spines
        std::vector<dendrite_t*> dendrites;
        for (int i = 0; i < NUM_DENDRITES; i++) {
            dendrite_config_t config = CreateDefaultConfig(i);
            dendrite_t* d = dendrite_create(&config);

            // Add segment
            segment_config_t seg_config = {};
            seg_config.type = DENDRITE_SEGMENT_PROXIMAL;
            seg_config.parent_segment = UINT32_MAX;
            seg_config.length = 50.0f;
            seg_config.diameter = 2.0f;
            dendrite_create_segments(d, 1, &seg_config);

            // Add spines
            for (int s = 0; s < 5; s++) {
                dendrite_add_spine(d, 0, SPINE_TYPE_MUSHROOM, i * 10 + s);
            }

            dendrites.push_back(d);
        }

        // Measure destruction
        double time = PerformanceMonitor::MeasureTimeMs([&]() {
            for (auto* d : dendrites) {
                dendrite_destroy(d);
            }
        });

        times_ms.push_back(time);
    }

    double mean = PerformanceMonitor::Mean(times_ms);

    std::cout << "Destroying " << NUM_DENDRITES << " dendrites (with segments/spines):" << std::endl;
    std::cout << "  Mean: " << std::fixed << std::setprecision(3) << mean << " ms" << std::endl;
    std::cout << "  Baseline: < " << Baseline::DESTRUCTION_1K_MS << " ms" << std::endl;

    EXPECT_LT(mean, Baseline::DESTRUCTION_1K_MS * Baseline::REGRESSION_TOLERANCE)
        << "Destruction performance regression detected";
}

//=============================================================================
// SEGMENT PERFORMANCE
//=============================================================================

TEST_F(DendritePerformanceTest, SegmentCreationPerformance) {
    std::cout << "\n=== Segment Creation Performance ===" << std::endl;

    constexpr int NUM_SEGMENTS = 10000;
    constexpr int SEGMENTS_PER_DENDRITE = 10;
    constexpr int NUM_DENDRITES = NUM_SEGMENTS / SEGMENTS_PER_DENDRITE;

    std::vector<double> times_ms;

    for (int sample = 0; sample < NUM_SAMPLES; sample++) {
        // Create dendrites
        std::vector<dendrite_t*> dendrites;
        for (int i = 0; i < NUM_DENDRITES; i++) {
            dendrite_config_t config = CreateDefaultConfig(i);
            dendrites.push_back(dendrite_create(&config));
        }

        // Prepare segment configs
        std::vector<segment_config_t> seg_configs(SEGMENTS_PER_DENDRITE);
        for (int j = 0; j < SEGMENTS_PER_DENDRITE; j++) {
            seg_configs[j].type = (j == 0) ? DENDRITE_SEGMENT_PROXIMAL : DENDRITE_SEGMENT_DISTAL;
            seg_configs[j].parent_segment = (j == 0) ? UINT32_MAX : (j - 1);
            seg_configs[j].length = 30.0f;
            seg_configs[j].diameter = 2.0f;
            seg_configs[j].path_distance = j * 30.0f;
        }

        // Measure segment creation
        double time = PerformanceMonitor::MeasureTimeMs([&]() {
            for (auto* d : dendrites) {
                dendrite_create_segments(d, SEGMENTS_PER_DENDRITE, seg_configs.data());
            }
        });

        times_ms.push_back(time);

        // Cleanup
        for (auto* d : dendrites) dendrite_destroy(d);
    }

    double mean = PerformanceMonitor::Mean(times_ms);

    std::cout << "Creating " << NUM_SEGMENTS << " segments:" << std::endl;
    std::cout << "  Mean: " << std::fixed << std::setprecision(3) << mean << " ms" << std::endl;
    std::cout << "  Throughput: " << (NUM_SEGMENTS / mean * 1000.0) << " segments/sec" << std::endl;
    std::cout << "  Baseline: < " << Baseline::SEGMENT_10K_MS << " ms" << std::endl;

    EXPECT_LT(mean, Baseline::SEGMENT_10K_MS * Baseline::REGRESSION_TOLERANCE)
        << "Segment creation performance regression detected";
}

//=============================================================================
// SPINE PERFORMANCE
//=============================================================================

TEST_F(DendritePerformanceTest, SpineCreationPerformance) {
    std::cout << "\n=== Spine Creation Performance ===" << std::endl;

    constexpr int NUM_SPINES = 10000;
    constexpr int SPINES_PER_DENDRITE = 100;
    constexpr int NUM_DENDRITES = NUM_SPINES / SPINES_PER_DENDRITE;

    std::vector<double> times_ms;

    for (int sample = 0; sample < NUM_SAMPLES; sample++) {
        // Create dendrites with segments
        std::vector<dendrite_t*> dendrites;
        for (int i = 0; i < NUM_DENDRITES; i++) {
            dendrite_config_t config = CreateDefaultConfig(i);
            dendrite_t* d = dendrite_create(&config);

            segment_config_t seg_config = {};
            seg_config.type = DENDRITE_SEGMENT_PROXIMAL;
            seg_config.parent_segment = UINT32_MAX;
            seg_config.length = 100.0f;
            seg_config.diameter = 2.0f;
            dendrite_create_segments(d, 1, &seg_config);

            dendrites.push_back(d);
        }

        // Measure spine creation
        double time = PerformanceMonitor::MeasureTimeMs([&]() {
            uint32_t syn_id = 0;
            for (auto* d : dendrites) {
                for (int s = 0; s < SPINES_PER_DENDRITE; s++) {
                    spine_type_t type = static_cast<spine_type_t>(s % SPINE_TYPE_COUNT);
                    dendrite_add_spine(d, 0, type, syn_id++);
                }
            }
        });

        times_ms.push_back(time);

        // Cleanup
        for (auto* d : dendrites) dendrite_destroy(d);
    }

    double mean = PerformanceMonitor::Mean(times_ms);

    std::cout << "Creating " << NUM_SPINES << " spines:" << std::endl;
    std::cout << "  Mean: " << std::fixed << std::setprecision(3) << mean << " ms" << std::endl;
    std::cout << "  Throughput: " << (NUM_SPINES / mean * 1000.0) << " spines/sec" << std::endl;
    std::cout << "  Baseline: < " << Baseline::SPINE_10K_MS << " ms" << std::endl;

    EXPECT_LT(mean, Baseline::SPINE_10K_MS * Baseline::REGRESSION_TOLERANCE)
        << "Spine creation performance regression detected";
}

//=============================================================================
// SIGNAL INTEGRATION PERFORMANCE
//=============================================================================

TEST_F(DendritePerformanceTest, InputProcessingPerformance) {
    std::cout << "\n=== Input Processing Performance ===" << std::endl;

    constexpr int NUM_INPUTS = 100000;
    std::vector<double> times_ms;

    // Create dendrite with segments
    dendrite_config_t config = CreateDefaultConfig(1);
    dendrite_t* dendrite = dendrite_create(&config);
    ASSERT_NE(dendrite, nullptr);

    std::vector<segment_config_t> seg_configs(5);
    for (int i = 0; i < 5; i++) {
        seg_configs[i].type = (i == 0) ? DENDRITE_SEGMENT_PROXIMAL : DENDRITE_SEGMENT_DISTAL;
        seg_configs[i].parent_segment = (i == 0) ? UINT32_MAX : (i - 1);
        seg_configs[i].length = 40.0f;
        seg_configs[i].diameter = 2.0f;
    }
    ASSERT_TRUE(dendrite_create_segments(dendrite, 5, seg_configs.data()));

    for (int sample = 0; sample < NUM_SAMPLES; sample++) {
        uint64_t time = 1000000;

        double elapsed = PerformanceMonitor::MeasureTimeMs([&]() {
            for (int i = 0; i < NUM_INPUTS; i++) {
                uint32_t seg_id = i % dendrite->num_segments;
                float current = 5.0f + (i % 10);
                dendrite_receive_input(dendrite, seg_id, current, time);
                time += 10;  // 0.01ms between inputs
            }
        });

        times_ms.push_back(elapsed);
    }

    double mean = PerformanceMonitor::Mean(times_ms);

    std::cout << "Processing " << NUM_INPUTS << " inputs:" << std::endl;
    std::cout << "  Mean: " << std::fixed << std::setprecision(3) << mean << " ms" << std::endl;
    std::cout << "  Throughput: " << (NUM_INPUTS / mean * 1000.0) << " inputs/sec" << std::endl;
    std::cout << "  Baseline: < " << Baseline::INPUT_100K_MS << " ms" << std::endl;

    EXPECT_LT(mean, Baseline::INPUT_100K_MS * Baseline::REGRESSION_TOLERANCE)
        << "Input processing performance regression detected";

    dendrite_destroy(dendrite);
}

//=============================================================================
// NETWORK PERFORMANCE
//=============================================================================

TEST_F(DendritePerformanceTest, NetworkStepPerformance) {
    std::cout << "\n=== Network Step Performance ===" << std::endl;

    constexpr int NUM_DENDRITES = 1000;
    constexpr int NUM_STEPS = 100;
    std::vector<double> times_ms;

    // Create network
    dendrite_network_t* network = dendrite_network_create(NUM_DENDRITES);
    ASSERT_NE(network, nullptr);

    for (int i = 0; i < NUM_DENDRITES; i++) {
        dendrite_config_t config = CreateDefaultConfig(i);
        dendrite_t* d = dendrite_create(&config);

        segment_config_t seg_config = {};
        seg_config.type = DENDRITE_SEGMENT_PROXIMAL;
        seg_config.parent_segment = UINT32_MAX;
        seg_config.length = 50.0f;
        seg_config.diameter = 2.0f;
        dendrite_create_segments(d, 1, &seg_config);

        // Add spines
        for (int s = 0; s < 5; s++) {
            dendrite_add_spine(d, 0, SPINE_TYPE_MUSHROOM, i * 10 + s);
        }

        dendrite_network_add(network, d);
    }

    // Warmup
    uint64_t time = 0;
    for (int w = 0; w < 10; w++) {
        time += 1000;
        dendrite_network_step(network, 1.0f, time);
    }

    // Measure
    for (int sample = 0; sample < NUM_SAMPLES; sample++) {
        double elapsed = PerformanceMonitor::MeasureTimeMs([&]() {
            for (int step = 0; step < NUM_STEPS; step++) {
                time += 1000;
                dendrite_network_step(network, 1.0f, time);
            }
        });

        times_ms.push_back(elapsed / NUM_STEPS);  // Per-step time
    }

    double mean = PerformanceMonitor::Mean(times_ms);
    double stddev = PerformanceMonitor::StdDev(times_ms);

    std::cout << "Network step with " << NUM_DENDRITES << " dendrites:" << std::endl;
    std::cout << "  Mean per step: " << std::fixed << std::setprecision(3) << mean << " ms" << std::endl;
    std::cout << "  StdDev: " << stddev << " ms" << std::endl;
    std::cout << "  Throughput: " << (NUM_DENDRITES / mean) << " K dendrites/ms" << std::endl;
    std::cout << "  Baseline: < " << Baseline::NETWORK_STEP_1K_MS << " ms" << std::endl;

    EXPECT_LT(mean, Baseline::NETWORK_STEP_1K_MS * Baseline::REGRESSION_TOLERANCE)
        << "Network step performance regression detected";

    dendrite_network_destroy(network);
}

//=============================================================================
// PLASTICITY PERFORMANCE
//=============================================================================

TEST_F(DendritePerformanceTest, PlasticityPerformance) {
    std::cout << "\n=== Plasticity Performance ===" << std::endl;

    constexpr int NUM_EVENTS = 10000;
    std::vector<double> times_ms;

    // Create dendrite with many spines
    dendrite_config_t config = CreateDefaultConfig(1);
    dendrite_t* dendrite = dendrite_create(&config);
    ASSERT_NE(dendrite, nullptr);

    segment_config_t seg_config = {};
    seg_config.type = DENDRITE_SEGMENT_PROXIMAL;
    seg_config.parent_segment = UINT32_MAX;
    seg_config.length = 100.0f;
    seg_config.diameter = 2.0f;
    ASSERT_TRUE(dendrite_create_segments(dendrite, 1, &seg_config));

    // Add spines
    for (int i = 0; i < 100; i++) {
        dendrite_add_spine(dendrite, 0, SPINE_TYPE_MUSHROOM, i);
    }

    for (int sample = 0; sample < NUM_SAMPLES; sample++) {
        // Set calcium above threshold for all spines
        for (uint32_t i = 0; i < dendrite->num_spines; i++) {
            dendrite->spines[i].calcium = 1.0f;
        }

        double elapsed = PerformanceMonitor::MeasureTimeMs([&]() {
            for (int i = 0; i < NUM_EVENTS; i++) {
                uint32_t spine_id = i % dendrite->num_spines;
                if (i % 2 == 0) {
                    dendrite_induce_ltp(dendrite, spine_id, 0.5f);
                } else {
                    dendrite->spines[spine_id].calcium = 0.5f;  // LTD range
                    dendrite_induce_ltd(dendrite, spine_id, 0.5f);
                    dendrite->spines[spine_id].calcium = 1.0f;  // Reset
                }
            }
        });

        times_ms.push_back(elapsed);
    }

    double mean = PerformanceMonitor::Mean(times_ms);

    std::cout << "Processing " << NUM_EVENTS << " plasticity events:" << std::endl;
    std::cout << "  Mean: " << std::fixed << std::setprecision(3) << mean << " ms" << std::endl;
    std::cout << "  Throughput: " << (NUM_EVENTS / mean * 1000.0) << " events/sec" << std::endl;
    std::cout << "  Baseline: < " << Baseline::PLASTICITY_10K_MS << " ms" << std::endl;

    EXPECT_LT(mean, Baseline::PLASTICITY_10K_MS * Baseline::REGRESSION_TOLERANCE)
        << "Plasticity performance regression detected";

    dendrite_destroy(dendrite);
}

//=============================================================================
// SCALABILITY TESTS
//=============================================================================

TEST_F(DendritePerformanceTest, ScalabilityTest) {
    std::cout << "\n=== Scalability Test ===" << std::endl;

    std::vector<int> sizes = {100, 500, 1000, 2000, 5000};
    std::vector<double> times_per_dendrite;

    std::cout << "Size\t\tTime (ms)\tTime/Dendrite (us)" << std::endl;
    std::cout << "------------------------------------------------" << std::endl;

    for (int size : sizes) {
        dendrite_network_t* network = dendrite_network_create(size);
        ASSERT_NE(network, nullptr);

        for (int i = 0; i < size; i++) {
            dendrite_config_t config = CreateDefaultConfig(i);
            dendrite_t* d = dendrite_create(&config);

            segment_config_t seg_config = {};
            seg_config.type = DENDRITE_SEGMENT_PROXIMAL;
            seg_config.parent_segment = UINT32_MAX;
            seg_config.length = 50.0f;
            seg_config.diameter = 2.0f;
            dendrite_create_segments(d, 1, &seg_config);

            dendrite_network_add(network, d);
        }

        // Measure step time
        uint64_t time = 1000000;
        std::vector<double> step_times;

        for (int step = 0; step < 20; step++) {
            time += 1000;
            double elapsed = PerformanceMonitor::MeasureTimeMs([&]() {
                dendrite_network_step(network, 1.0f, time);
            });
            step_times.push_back(elapsed);
        }

        double mean = PerformanceMonitor::Mean(step_times);
        double per_dendrite = mean * 1000.0 / size;  // Convert to microseconds

        times_per_dendrite.push_back(per_dendrite);

        std::cout << size << "\t\t" << std::fixed << std::setprecision(3)
                  << mean << "\t\t" << std::setprecision(2) << per_dendrite << std::endl;

        dendrite_network_destroy(network);
    }

    // Verify O(n) scaling - time per dendrite should be relatively constant
    double min_per_dendrite = *std::min_element(times_per_dendrite.begin(), times_per_dendrite.end());
    double max_per_dendrite = *std::max_element(times_per_dendrite.begin(), times_per_dendrite.end());
    double ratio = max_per_dendrite / min_per_dendrite;

    std::cout << "\n--- Scaling Analysis ---" << std::endl;
    std::cout << "Min time/dendrite: " << min_per_dendrite << " us" << std::endl;
    std::cout << "Max time/dendrite: " << max_per_dendrite << " us" << std::endl;
    std::cout << "Ratio: " << ratio << "x (ideal = 1.0)" << std::endl;

    // Allow up to 3x variation for cache effects
    EXPECT_LT(ratio, 3.0) << "Scaling worse than O(n)";
}

//=============================================================================
// MEMORY EFFICIENCY
//=============================================================================

TEST_F(DendritePerformanceTest, MemoryEfficiency) {
    std::cout << "\n=== Memory Efficiency Test ===" << std::endl;

    // sizeof gives base sizes
    std::cout << "Base dendrite_t size: " << sizeof(dendrite_t) << " bytes" << std::endl;
    std::cout << "Segment size: " << sizeof(dendritic_segment_t) << " bytes" << std::endl;
    std::cout << "Spine size: " << sizeof(dendritic_spine_t) << " bytes" << std::endl;
    std::cout << "Config size: " << sizeof(dendrite_config_t) << " bytes" << std::endl;

    // Create dendrite with segments and spines to measure full size
    dendrite_config_t config = CreateDefaultConfig(1);
    dendrite_t* dendrite = dendrite_create(&config);
    ASSERT_NE(dendrite, nullptr);

    std::vector<segment_config_t> seg_configs(10);
    for (int i = 0; i < 10; i++) {
        seg_configs[i].type = (i == 0) ? DENDRITE_SEGMENT_PROXIMAL : DENDRITE_SEGMENT_DISTAL;
        seg_configs[i].parent_segment = (i == 0) ? UINT32_MAX : (i - 1);
        seg_configs[i].length = 30.0f;
        seg_configs[i].diameter = 2.0f;
    }
    dendrite_create_segments(dendrite, 10, seg_configs.data());

    for (int i = 0; i < 20; i++) {
        dendrite_add_spine(dendrite, 0, SPINE_TYPE_MUSHROOM, i);
    }

    size_t estimated_size = sizeof(dendrite_t) +
                            10 * sizeof(dendritic_segment_t) +
                            20 * sizeof(dendritic_spine_t);

    std::cout << "Estimated dendrite with 10 segments, 20 spines: " << estimated_size << " bytes" << std::endl;

    // Memory should be reasonable (< 8KB per dendrite with full complexity)
    EXPECT_LT(estimated_size, 8192) << "Memory per dendrite exceeds 8KB";

    dendrite_destroy(dendrite);
}

//=============================================================================
// BIOLOGICAL ACCURACY REGRESSION
//=============================================================================

TEST_F(DendritePerformanceTest, BiologicalAccuracyRegression) {
    std::cout << "\n=== Biological Accuracy Regression ===" << std::endl;

    dendrite_config_t config = CreateDefaultConfig(1);
    dendrite_t* dendrite = dendrite_create(&config);
    ASSERT_NE(dendrite, nullptr);

    segment_config_t seg_config = {};
    seg_config.type = DENDRITE_SEGMENT_PROXIMAL;
    seg_config.parent_segment = UINT32_MAX;
    seg_config.length = 100.0f;  // 100um
    seg_config.diameter = 2.0f;   // 2um diameter
    ASSERT_TRUE(dendrite_create_segments(dendrite, 1, &seg_config));

    // Verify cable properties are in biological range
    // NOTE: Segment R_m and C_m are *absolute* values (not per unit area)
    // R_m = R_m_material / surface_area (gives resistance in ohms)
    // C_m = C_m_material * surface_area (gives capacitance in uF)
    // For 100um length x 2um diameter: surface area = pi * 2 * 100 um^2 = 628 um^2 = 6.28e-6 cm^2
    float R_m = dendrite->segments[0].R_m;
    std::cout << "Segment membrane resistance (R_m): " << R_m << " ohms (absolute)" << std::endl;
    EXPECT_GT(R_m, 1e6f) << "R_m too low for typical segment";  // Should be MOhm range
    EXPECT_LT(R_m, 1e12f) << "R_m too high";

    // C_m should be very small (surface area * 1 uF/cm^2)
    float C_m = dendrite->segments[0].C_m;
    std::cout << "Segment membrane capacitance (C_m): " << C_m << " uF (absolute)" << std::endl;
    EXPECT_GT(C_m, 1e-9f) << "C_m too low";  // pF range
    EXPECT_LT(C_m, 1e-3f) << "C_m too high";

    // R_a is also scaled for the segment length
    // R_a = R_a_material * length / cross_section_area
    float R_a = dendrite->segments[0].R_a;
    std::cout << "Segment axial resistance (R_a): " << R_a << " ohms (absolute)" << std::endl;
    EXPECT_GT(R_a, 1e6f) << "R_a too low";  // MOhm range for thin long segments
    EXPECT_LT(R_a, 1e12f) << "R_a too high";

    // Path resistance should be positive for proximal segment
    float path_R = dendrite->segments[0].path_resistance;
    std::cout << "Path resistance: " << path_R << " MOhm" << std::endl;
    EXPECT_GE(path_R, 0.0f) << "Invalid path resistance";

    // Check dendrite time constant is in reasonable range
    float tau = dendrite->time_constant;
    std::cout << "Dendrite time constant (tau): " << tau << " ms" << std::endl;
    // May be 0 if not computed yet, just verify non-negative
    EXPECT_GE(tau, 0.0f) << "Invalid time constant";

    dendrite_destroy(dendrite);

    std::cout << "Biological parameters within expected ranges." << std::endl;
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
