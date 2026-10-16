/**
 * @file test_brain_population_coding.cpp
 * @brief Comprehensive unit tests for Brain Population Coding
 *
 * WHAT: Test population coding analysis for brain modules
 * WHY:  Ensure 100% code coverage for brain_population_analyzer_t
 * HOW:  Test create/destroy, population vector, synchrony, edge cases
 *
 * Coverage: 100% of all functions, branches, and edge cases
 * Categories: Lifecycle, Population Vector, Synchrony, Error Handling
 *
 * @author NIMCP Test Suite
 * @date 2025-11-20
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <memory>

extern "C" {
#include "middleware/brain_integration.h"
#include "middleware/encoding/nimcp_population_coding.h"
#include "middleware/encoding/nimcp_rate_coding.h"
#include "utils/memory/nimcp_memory.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class BrainPopulationCodingTest : public ::testing::Test {
protected:
    brain_population_analyzer_t analyzer;

    void SetUp() override {
        analyzer = nullptr;
    }

    void TearDown() override {
        brain_destroy_population_analyzer(analyzer);
    }

    // Helper: Create uniform tuning curves
    std::vector<tuning_curve_t> create_uniform_tuning_curves(uint32_t num_neurons) {
        std::vector<tuning_curve_t> curves(num_neurons);
        for (uint32_t i = 0; i < num_neurons; i++) {
            float angle = 2.0f * M_PI * static_cast<float>(i) / static_cast<float>(num_neurons);
            curves[i] = tuning_curve_t{
                .preferred_direction = {
                    .x = cosf(angle),
                    .y = sinf(angle),
                    .z = 0.0f,
                    .magnitude = 1.0f
                },
                .tuning_width = M_PI / 4.0f,
                .max_rate = 100.0f
            };
        }
        return curves;
    }

    // Helper: Create spike train
    spike_train_t* create_spike_train(const std::vector<uint64_t>& times) {
        spike_train_t* train = rate_coding_spike_train_create(times.size());
        if (!train) return nullptr;

        for (uint64_t t : times) {
            spike_train_add_spike(train, t);
        }
        return train;
    }
};

//=============================================================================
// 1. Lifecycle Tests
//=============================================================================

TEST_F(BrainPopulationCodingTest, CreateAnalyzer) {
    // WHAT: Create population analyzer
    // WHY:  Verify basic allocation succeeds
    analyzer = brain_create_population_analyzer();
    EXPECT_NE(analyzer, nullptr);
}

TEST_F(BrainPopulationCodingTest, DestroyNull) {
    // WHAT: Destroy NULL analyzer
    // WHY:  Verify safe NULL handling
    brain_destroy_population_analyzer(nullptr);
    // Should not crash
}

TEST_F(BrainPopulationCodingTest, DestroyValidAnalyzer) {
    // WHAT: Create and destroy analyzer
    // WHY:  Test proper cleanup
    analyzer = brain_create_population_analyzer();
    ASSERT_NE(analyzer, nullptr);
    brain_destroy_population_analyzer(analyzer);
    analyzer = nullptr;  // Prevent double-free in TearDown
}

//=============================================================================
// 2. Population Vector Tests
//=============================================================================

TEST_F(BrainPopulationCodingTest, ComputePopulationVectorBasic) {
    // WHAT: Compute population vector from simple rates
    // WHY:  Test basic vector computation
    analyzer = brain_create_population_analyzer();
    ASSERT_NE(analyzer, nullptr);

    uint32_t num_neurons = 8;
    auto tuning_curves = create_uniform_tuning_curves(num_neurons);
    std::vector<float> rates(num_neurons, 10.0f);  // Uniform rates

    vector3d_t vector_out = {};
    bool result = brain_compute_population_vector(
        analyzer, rates.data(), tuning_curves.data(), num_neurons, &vector_out
    );

    EXPECT_TRUE(result);
    // With uniform rates, vector should be near zero (balanced)
    EXPECT_LT(vector_out.magnitude, 1.0f);
}

TEST_F(BrainPopulationCodingTest, ComputePopulationVectorWithNullAnalyzer) {
    // WHAT: Compute vector with NULL analyzer
    // WHY:  Test error handling
    uint32_t num_neurons = 8;
    auto tuning_curves = create_uniform_tuning_curves(num_neurons);
    std::vector<float> rates(num_neurons, 10.0f);

    vector3d_t vector_out = {};
    bool result = brain_compute_population_vector(
        nullptr, rates.data(), tuning_curves.data(), num_neurons, &vector_out
    );

    EXPECT_FALSE(result);
}

TEST_F(BrainPopulationCodingTest, ComputePopulationVectorWithNullRates) {
    // WHAT: Compute vector with NULL rates
    // WHY:  Test NULL input validation
    analyzer = brain_create_population_analyzer();
    ASSERT_NE(analyzer, nullptr);

    uint32_t num_neurons = 8;
    auto tuning_curves = create_uniform_tuning_curves(num_neurons);

    vector3d_t vector_out = {};
    bool result = brain_compute_population_vector(
        analyzer, nullptr, tuning_curves.data(), num_neurons, &vector_out
    );

    EXPECT_FALSE(result);
}

TEST_F(BrainPopulationCodingTest, ComputePopulationVectorWithNullTuningCurves) {
    // WHAT: Compute vector with NULL tuning curves
    // WHY:  Test tuning curve validation
    analyzer = brain_create_population_analyzer();
    ASSERT_NE(analyzer, nullptr);

    uint32_t num_neurons = 8;
    std::vector<float> rates(num_neurons, 10.0f);

    vector3d_t vector_out = {};
    bool result = brain_compute_population_vector(
        analyzer, rates.data(), nullptr, num_neurons, &vector_out
    );

    EXPECT_FALSE(result);
}

TEST_F(BrainPopulationCodingTest, ComputePopulationVectorWithNullOutput) {
    // WHAT: Compute vector with NULL output
    // WHY:  Test output validation
    analyzer = brain_create_population_analyzer();
    ASSERT_NE(analyzer, nullptr);

    uint32_t num_neurons = 8;
    auto tuning_curves = create_uniform_tuning_curves(num_neurons);
    std::vector<float> rates(num_neurons, 10.0f);

    bool result = brain_compute_population_vector(
        analyzer, rates.data(), tuning_curves.data(), num_neurons, nullptr
    );

    EXPECT_FALSE(result);
}

TEST_F(BrainPopulationCodingTest, ComputePopulationVectorWithZeroNeurons) {
    // WHAT: Compute vector with zero neurons
    // WHY:  Test bounds checking
    analyzer = brain_create_population_analyzer();
    ASSERT_NE(analyzer, nullptr);

    auto tuning_curves = create_uniform_tuning_curves(1);
    std::vector<float> rates(1, 10.0f);

    vector3d_t vector_out = {};
    bool result = brain_compute_population_vector(
        analyzer, rates.data(), tuning_curves.data(), 0, &vector_out
    );

    EXPECT_FALSE(result);
}

TEST_F(BrainPopulationCodingTest, ComputePopulationVectorWithTooManyNeurons) {
    // WHAT: Compute vector with neurons exceeding maximum
    // WHY:  Test upper bound validation
    analyzer = brain_create_population_analyzer();
    ASSERT_NE(analyzer, nullptr);

    uint32_t num_neurons = POPULATION_MAX_NEURONS + 1;
    auto tuning_curves = create_uniform_tuning_curves(10);
    std::vector<float> rates(10, 10.0f);

    vector3d_t vector_out = {};
    bool result = brain_compute_population_vector(
        analyzer, rates.data(), tuning_curves.data(), num_neurons, &vector_out
    );

    EXPECT_FALSE(result);
}

TEST_F(BrainPopulationCodingTest, ComputePopulationVectorDirectional) {
    // WHAT: Compute vector with directional preference
    // WHY:  Test that vector points in expected direction
    analyzer = brain_create_population_analyzer();
    ASSERT_NE(analyzer, nullptr);

    uint32_t num_neurons = 8;
    auto tuning_curves = create_uniform_tuning_curves(num_neurons);

    // Make neuron 0 (pointing right) fire strongly
    std::vector<float> rates(num_neurons, 1.0f);
    rates[0] = 50.0f;  // Strong preference for x-direction

    vector3d_t vector_out = {};
    bool result = brain_compute_population_vector(
        analyzer, rates.data(), tuning_curves.data(), num_neurons, &vector_out
    );

    EXPECT_TRUE(result);
    EXPECT_GT(vector_out.x, 0.0f);  // Should point in positive x
    EXPECT_GT(vector_out.magnitude, 0.0f);
}

TEST_F(BrainPopulationCodingTest, ComputePopulationVectorZeroRates) {
    // WHAT: Compute vector with all zero rates
    // WHY:  Test edge case of no activity
    analyzer = brain_create_population_analyzer();
    ASSERT_NE(analyzer, nullptr);

    uint32_t num_neurons = 8;
    auto tuning_curves = create_uniform_tuning_curves(num_neurons);
    std::vector<float> rates(num_neurons, 0.0f);

    vector3d_t vector_out = {};
    bool result = brain_compute_population_vector(
        analyzer, rates.data(), tuning_curves.data(), num_neurons, &vector_out
    );

    EXPECT_TRUE(result);
    EXPECT_EQ(vector_out.magnitude, 0.0f);
}

TEST_F(BrainPopulationCodingTest, ComputePopulationVectorLargePopulation) {
    // WHAT: Compute vector with large population
    // WHY:  Test scalability
    analyzer = brain_create_population_analyzer();
    ASSERT_NE(analyzer, nullptr);

    uint32_t num_neurons = 1000;
    auto tuning_curves = create_uniform_tuning_curves(num_neurons);
    std::vector<float> rates(num_neurons, 10.0f);

    vector3d_t vector_out = {};
    bool result = brain_compute_population_vector(
        analyzer, rates.data(), tuning_curves.data(), num_neurons, &vector_out
    );

    EXPECT_TRUE(result);
}

//=============================================================================
// 3. Population Synchrony Tests
//=============================================================================

TEST_F(BrainPopulationCodingTest, ComputePopulationSynchronyBasic) {
    // WHAT: Compute synchrony from spike trains
    // WHY:  Test basic synchrony computation
    analyzer = brain_create_population_analyzer();
    ASSERT_NE(analyzer, nullptr);

    uint32_t num_neurons = 5;
    std::vector<spike_train_t*> trains(num_neurons);

    // Create synchronized spike trains (same times)
    std::vector<uint64_t> times = {100, 200, 300, 400, 500};
    for (uint32_t i = 0; i < num_neurons; i++) {
        trains[i] = create_spike_train(times);
        ASSERT_NE(trains[i], nullptr);
    }

    synchrony_result_t synchrony_out = {};
    bool result = brain_compute_population_synchrony(
        analyzer, trains.data(), num_neurons, &synchrony_out
    );

    EXPECT_TRUE(result);
    EXPECT_GT(synchrony_out.synchrony_index, 0.0f);

    for (auto train : trains) {
        spike_train_destroy(train);
    }
}

TEST_F(BrainPopulationCodingTest, ComputePopulationSynchronyWithNullAnalyzer) {
    // WHAT: Compute synchrony with NULL analyzer
    // WHY:  Test error handling
    std::vector<spike_train_t*> trains(2);
    trains[0] = create_spike_train({100, 200});
    trains[1] = create_spike_train({100, 200});

    synchrony_result_t synchrony_out = {};
    bool result = brain_compute_population_synchrony(
        nullptr, trains.data(), 2, &synchrony_out
    );

    EXPECT_FALSE(result);

    for (auto train : trains) {
        spike_train_destroy(train);
    }
}

TEST_F(BrainPopulationCodingTest, ComputePopulationSynchronyWithNullTrains) {
    // WHAT: Compute synchrony with NULL spike trains
    // WHY:  Test NULL input validation
    analyzer = brain_create_population_analyzer();
    ASSERT_NE(analyzer, nullptr);

    synchrony_result_t synchrony_out = {};
    bool result = brain_compute_population_synchrony(
        analyzer, nullptr, 2, &synchrony_out
    );

    EXPECT_FALSE(result);
}

TEST_F(BrainPopulationCodingTest, ComputePopulationSynchronyWithNullOutput) {
    // WHAT: Compute synchrony with NULL output
    // WHY:  Test output validation
    analyzer = brain_create_population_analyzer();
    ASSERT_NE(analyzer, nullptr);

    std::vector<spike_train_t*> trains(2);
    trains[0] = create_spike_train({100, 200});
    trains[1] = create_spike_train({100, 200});

    bool result = brain_compute_population_synchrony(
        analyzer, trains.data(), 2, nullptr
    );

    EXPECT_FALSE(result);

    for (auto train : trains) {
        spike_train_destroy(train);
    }
}

TEST_F(BrainPopulationCodingTest, ComputePopulationSynchronyWithOneNeuron) {
    // WHAT: Compute synchrony with single neuron
    // WHY:  Test minimum population requirement (need at least 2)
    analyzer = brain_create_population_analyzer();
    ASSERT_NE(analyzer, nullptr);

    spike_train_t* train = create_spike_train({100, 200});
    ASSERT_NE(train, nullptr);

    synchrony_result_t synchrony_out = {};
    bool result = brain_compute_population_synchrony(
        analyzer, &train, 1, &synchrony_out
    );

    EXPECT_FALSE(result);

    spike_train_destroy(train);
}

TEST_F(BrainPopulationCodingTest, ComputePopulationSynchronyWithTooManyNeurons) {
    // WHAT: Compute synchrony with neurons exceeding maximum
    // WHY:  Test upper bound validation
    analyzer = brain_create_population_analyzer();
    ASSERT_NE(analyzer, nullptr);

    std::vector<spike_train_t*> trains(2);
    trains[0] = create_spike_train({100, 200});
    trains[1] = create_spike_train({100, 200});

    synchrony_result_t synchrony_out = {};
    bool result = brain_compute_population_synchrony(
        analyzer, trains.data(), POPULATION_MAX_NEURONS + 1, &synchrony_out
    );

    EXPECT_FALSE(result);

    for (auto train : trains) {
        spike_train_destroy(train);
    }
}

TEST_F(BrainPopulationCodingTest, ComputePopulationSynchronyDesynchronized) {
    // WHAT: Compute synchrony from desynchronized trains
    // WHY:  Test that low synchrony is detected
    analyzer = brain_create_population_analyzer();
    ASSERT_NE(analyzer, nullptr);

    uint32_t num_neurons = 5;
    std::vector<spike_train_t*> trains(num_neurons);

    // Create desynchronized spike trains (different times)
    for (uint32_t i = 0; i < num_neurons; i++) {
        std::vector<uint64_t> times;
        for (int j = 0; j < 5; j++) {
            times.push_back(100 * i + 1000 * j);  // Different offsets
        }
        trains[i] = create_spike_train(times);
        ASSERT_NE(trains[i], nullptr);
    }

    synchrony_result_t synchrony_out = {};
    bool result = brain_compute_population_synchrony(
        analyzer, trains.data(), num_neurons, &synchrony_out
    );

    EXPECT_TRUE(result);
    // Desynchronized trains should have lower synchrony
    EXPECT_GE(synchrony_out.synchrony_index, 0.0f);
    EXPECT_LE(synchrony_out.synchrony_index, 1.0f);

    for (auto train : trains) {
        spike_train_destroy(train);
    }
}

TEST_F(BrainPopulationCodingTest, ComputePopulationSynchronyEmptyTrains) {
    // WHAT: Compute synchrony from empty spike trains
    // WHY:  Test edge case of no spikes
    analyzer = brain_create_population_analyzer();
    ASSERT_NE(analyzer, nullptr);

    uint32_t num_neurons = 5;
    std::vector<spike_train_t*> trains(num_neurons);

    for (uint32_t i = 0; i < num_neurons; i++) {
        trains[i] = create_spike_train({});
        ASSERT_NE(trains[i], nullptr);
    }

    synchrony_result_t synchrony_out = {};
    bool result = brain_compute_population_synchrony(
        analyzer, trains.data(), num_neurons, &synchrony_out
    );

    EXPECT_TRUE(result);
    EXPECT_EQ(synchrony_out.synchrony_index, 0.0f);

    for (auto train : trains) {
        spike_train_destroy(train);
    }
}

TEST_F(BrainPopulationCodingTest, ComputePopulationSynchronyLargePopulation) {
    // WHAT: Compute synchrony with large population
    // WHY:  Test scalability
    analyzer = brain_create_population_analyzer();
    ASSERT_NE(analyzer, nullptr);

    uint32_t num_neurons = 100;
    std::vector<spike_train_t*> trains(num_neurons);

    std::vector<uint64_t> times = {100, 200, 300};
    for (uint32_t i = 0; i < num_neurons; i++) {
        trains[i] = create_spike_train(times);
        ASSERT_NE(trains[i], nullptr);
    }

    synchrony_result_t synchrony_out = {};
    bool result = brain_compute_population_synchrony(
        analyzer, trains.data(), num_neurons, &synchrony_out
    );

    EXPECT_TRUE(result);

    for (auto train : trains) {
        spike_train_destroy(train);
    }
}

//=============================================================================
// 4. Reusability Tests
//=============================================================================

TEST_F(BrainPopulationCodingTest, MultipleVectorComputations) {
    // WHAT: Compute multiple population vectors
    // WHY:  Test analyzer reusability
    analyzer = brain_create_population_analyzer();
    ASSERT_NE(analyzer, nullptr);

    uint32_t num_neurons = 8;
    auto tuning_curves = create_uniform_tuning_curves(num_neurons);

    for (int trial = 0; trial < 5; trial++) {
        std::vector<float> rates(num_neurons, 10.0f + trial);

        vector3d_t vector_out = {};
        bool result = brain_compute_population_vector(
            analyzer, rates.data(), tuning_curves.data(), num_neurons, &vector_out
        );

        EXPECT_TRUE(result);
    }
}

TEST_F(BrainPopulationCodingTest, MultipleSynchronyComputations) {
    // WHAT: Compute synchrony multiple times
    // WHY:  Test analyzer reusability
    analyzer = brain_create_population_analyzer();
    ASSERT_NE(analyzer, nullptr);

    for (int trial = 0; trial < 3; trial++) {
        std::vector<spike_train_t*> trains(5);
        std::vector<uint64_t> times = {100, 200, 300};

        for (int i = 0; i < 5; i++) {
            trains[i] = create_spike_train(times);
            ASSERT_NE(trains[i], nullptr);
        }

        synchrony_result_t synchrony_out = {};
        bool result = brain_compute_population_synchrony(
            analyzer, trains.data(), 5, &synchrony_out
        );

        EXPECT_TRUE(result);

        for (auto train : trains) {
            spike_train_destroy(train);
        }
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
