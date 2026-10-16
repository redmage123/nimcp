/**
 * @file test_brain_init_config.cpp
 * @brief Unit tests for brain initialization configuration getters
 *
 * WHAT: Tests for neuron count and sparsity configuration functions
 * WHY:  Ensure correct configuration values for all brain sizes
 * HOW:  Use GoogleTest framework with comprehensive coverage
 *
 * Functions tested:
 * - nimcp_brain_factory_get_neuron_count()
 * - nimcp_brain_factory_get_default_sparsity()
 */

#include <gtest/gtest.h>
#include <cmath>
#include <limits>

#include "core/brain/factory/nimcp_brain_factory.h"
#include "core/brain/nimcp_brain.h"
#include "nimcp.h"

//=============================================================================
// Test Constants
//=============================================================================

// Expected neuron counts from implementation
static const uint32_t EXPECTED_MICRO_NEURONS = 25;    // Ultra-lightweight for unit tests
static const uint32_t EXPECTED_TINY_NEURONS = 100;
static const uint32_t EXPECTED_SMALL_NEURONS = 500;
static const uint32_t EXPECTED_MEDIUM_NEURONS = 1000;
static const uint32_t EXPECTED_LARGE_NEURONS = 5000;
static const uint32_t EXPECTED_CUSTOM_NEURONS = 1000;
static const uint32_t EXPECTED_DEFAULT_NEURONS = 1000;

// Expected sparsity values from implementation
static const float EXPECTED_MICRO_SPARSITY = 0.60f;   // Lower sparsity for micro brains
static const float EXPECTED_TINY_SPARSITY = 0.70f;
static const float EXPECTED_SMALL_SPARSITY = 0.80f;
static const float EXPECTED_MEDIUM_SPARSITY = 0.85f;
static const float EXPECTED_LARGE_SPARSITY = 0.90f;
static const float EXPECTED_DEFAULT_SPARSITY = 0.80f;

// Floating point comparison tolerance
static const float FLOAT_EPSILON = 1e-6f;

//=============================================================================
// Neuron Count Tests
//=============================================================================

/**
 * WHAT: Test neuron count for MICRO brain size
 * WHY:  Verify ultra-lightweight configuration for unit tests and swarm drones
 */
TEST(BrainInitConfig, NeuronCount_Micro)
{
    uint32_t count = nimcp_brain_factory_get_neuron_count(BRAIN_SIZE_MICRO);
    EXPECT_EQ(count, EXPECTED_MICRO_NEURONS)
        << "MICRO brain should have " << EXPECTED_MICRO_NEURONS << " neurons";
}

/**
 * WHAT: Test neuron count for TINY brain size
 * WHY:  Verify smallest configuration is correct
 */
TEST(BrainInitConfig, NeuronCount_Tiny)
{
    uint32_t count = nimcp_brain_factory_get_neuron_count(BRAIN_SIZE_TINY);
    EXPECT_EQ(count, EXPECTED_TINY_NEURONS)
        << "TINY brain should have " << EXPECTED_TINY_NEURONS << " neurons";
}

/**
 * WHAT: Test neuron count for SMALL brain size
 * WHY:  Verify standard small configuration
 */
TEST(BrainInitConfig, NeuronCount_Small)
{
    uint32_t count = nimcp_brain_factory_get_neuron_count(BRAIN_SIZE_SMALL);
    EXPECT_EQ(count, EXPECTED_SMALL_NEURONS)
        << "SMALL brain should have " << EXPECTED_SMALL_NEURONS << " neurons";
}

/**
 * WHAT: Test neuron count for MEDIUM brain size
 * WHY:  Verify default/common configuration
 */
TEST(BrainInitConfig, NeuronCount_Medium)
{
    uint32_t count = nimcp_brain_factory_get_neuron_count(BRAIN_SIZE_MEDIUM);
    EXPECT_EQ(count, EXPECTED_MEDIUM_NEURONS)
        << "MEDIUM brain should have " << EXPECTED_MEDIUM_NEURONS << " neurons";
}

/**
 * WHAT: Test neuron count for LARGE brain size
 * WHY:  Verify largest configuration
 */
TEST(BrainInitConfig, NeuronCount_Large)
{
    uint32_t count = nimcp_brain_factory_get_neuron_count(BRAIN_SIZE_LARGE);
    EXPECT_EQ(count, EXPECTED_LARGE_NEURONS)
        << "LARGE brain should have " << EXPECTED_LARGE_NEURONS << " neurons";
}

/**
 * WHAT: Test neuron count for CUSTOM brain size
 * WHY:  Verify custom configuration defaults
 */
TEST(BrainInitConfig, NeuronCount_Custom)
{
    uint32_t count = nimcp_brain_factory_get_neuron_count(BRAIN_SIZE_CUSTOM);
    EXPECT_EQ(count, EXPECTED_CUSTOM_NEURONS)
        << "CUSTOM brain should default to " << EXPECTED_CUSTOM_NEURONS << " neurons";
}

/**
 * WHAT: Test neuron count ordering
 * WHY:  Verify sizes increase monotonically
 */
TEST(BrainInitConfig, NeuronCount_Ordering)
{
    uint32_t micro = nimcp_brain_factory_get_neuron_count(BRAIN_SIZE_MICRO);
    uint32_t tiny = nimcp_brain_factory_get_neuron_count(BRAIN_SIZE_TINY);
    uint32_t small = nimcp_brain_factory_get_neuron_count(BRAIN_SIZE_SMALL);
    uint32_t medium = nimcp_brain_factory_get_neuron_count(BRAIN_SIZE_MEDIUM);
    uint32_t large = nimcp_brain_factory_get_neuron_count(BRAIN_SIZE_LARGE);

    EXPECT_LT(micro, tiny) << "MICRO should have fewer neurons than TINY";
    EXPECT_LT(tiny, small) << "TINY should have fewer neurons than SMALL";
    EXPECT_LT(small, medium) << "SMALL should have fewer neurons than MEDIUM";
    EXPECT_LT(medium, large) << "MEDIUM should have fewer neurons than LARGE";
}

/**
 * WHAT: Test neuron count scaling factor
 * WHY:  Verify reasonable scaling between sizes
 */
TEST(BrainInitConfig, NeuronCount_ScalingFactor)
{
    uint32_t tiny = nimcp_brain_factory_get_neuron_count(BRAIN_SIZE_TINY);
    uint32_t small = nimcp_brain_factory_get_neuron_count(BRAIN_SIZE_SMALL);
    uint32_t medium = nimcp_brain_factory_get_neuron_count(BRAIN_SIZE_MEDIUM);

    // Verify scaling is reasonable (not too small, not too large)
    float tiny_to_small_ratio = static_cast<float>(small) / tiny;
    float small_to_medium_ratio = static_cast<float>(medium) / small;

    EXPECT_GT(tiny_to_small_ratio, 2.0f)
        << "SMALL should be at least 2x TINY";
    EXPECT_GT(small_to_medium_ratio, 1.5f)
        << "MEDIUM should be at least 1.5x SMALL";
}

/**
 * WHAT: Test neuron count is non-zero
 * WHY:  Verify all sizes return valid counts
 */
TEST(BrainInitConfig, NeuronCount_NonZero)
{
    EXPECT_GT(nimcp_brain_factory_get_neuron_count(BRAIN_SIZE_MICRO), 0u);
    EXPECT_GT(nimcp_brain_factory_get_neuron_count(BRAIN_SIZE_TINY), 0u);
    EXPECT_GT(nimcp_brain_factory_get_neuron_count(BRAIN_SIZE_SMALL), 0u);
    EXPECT_GT(nimcp_brain_factory_get_neuron_count(BRAIN_SIZE_MEDIUM), 0u);
    EXPECT_GT(nimcp_brain_factory_get_neuron_count(BRAIN_SIZE_LARGE), 0u);
    EXPECT_GT(nimcp_brain_factory_get_neuron_count(BRAIN_SIZE_CUSTOM), 0u);
}

/**
 * WHAT: Test neuron count with invalid enum cast
 * WHY:  Verify graceful handling of out-of-range values
 */
TEST(BrainInitConfig, NeuronCount_InvalidEnum)
{
    // Cast invalid value to brain_size_t
    brain_size_t invalid = static_cast<brain_size_t>(999);
    uint32_t count = nimcp_brain_factory_get_neuron_count(invalid);

    // Should return default value
    EXPECT_EQ(count, EXPECTED_DEFAULT_NEURONS)
        << "Invalid size should return default neuron count";
}

/**
 * WHAT: Test neuron count consistency
 * WHY:  Verify repeated calls return same value
 */
TEST(BrainInitConfig, NeuronCount_Consistency)
{
    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(nimcp_brain_factory_get_neuron_count(BRAIN_SIZE_MEDIUM),
                  EXPECTED_MEDIUM_NEURONS)
            << "Neuron count should be consistent across calls";
    }
}

//=============================================================================
// Sparsity Tests
//=============================================================================

/**
 * WHAT: Test sparsity for MICRO brain size
 * WHY:  Verify ultra-lightweight networks have lowest sparsity (more dense connections)
 */
TEST(BrainInitConfig, Sparsity_Micro)
{
    float sparsity = nimcp_brain_factory_get_default_sparsity(BRAIN_SIZE_MICRO);
    EXPECT_NEAR(sparsity, EXPECTED_MICRO_SPARSITY, FLOAT_EPSILON)
        << "MICRO brain sparsity should be " << EXPECTED_MICRO_SPARSITY;
}

/**
 * WHAT: Test sparsity for TINY brain size
 * WHY:  Verify smallest networks have lowest sparsity
 */
TEST(BrainInitConfig, Sparsity_Tiny)
{
    float sparsity = nimcp_brain_factory_get_default_sparsity(BRAIN_SIZE_TINY);
    EXPECT_NEAR(sparsity, EXPECTED_TINY_SPARSITY, FLOAT_EPSILON)
        << "TINY brain sparsity should be " << EXPECTED_TINY_SPARSITY;
}

/**
 * WHAT: Test sparsity for SMALL brain size
 * WHY:  Verify standard small network sparsity
 */
TEST(BrainInitConfig, Sparsity_Small)
{
    float sparsity = nimcp_brain_factory_get_default_sparsity(BRAIN_SIZE_SMALL);
    EXPECT_NEAR(sparsity, EXPECTED_SMALL_SPARSITY, FLOAT_EPSILON)
        << "SMALL brain sparsity should be " << EXPECTED_SMALL_SPARSITY;
}

/**
 * WHAT: Test sparsity for MEDIUM brain size
 * WHY:  Verify medium network sparsity
 */
TEST(BrainInitConfig, Sparsity_Medium)
{
    float sparsity = nimcp_brain_factory_get_default_sparsity(BRAIN_SIZE_MEDIUM);
    EXPECT_NEAR(sparsity, EXPECTED_MEDIUM_SPARSITY, FLOAT_EPSILON)
        << "MEDIUM brain sparsity should be " << EXPECTED_MEDIUM_SPARSITY;
}

/**
 * WHAT: Test sparsity for LARGE brain size
 * WHY:  Verify largest networks have highest sparsity
 */
TEST(BrainInitConfig, Sparsity_Large)
{
    float sparsity = nimcp_brain_factory_get_default_sparsity(BRAIN_SIZE_LARGE);
    EXPECT_NEAR(sparsity, EXPECTED_LARGE_SPARSITY, FLOAT_EPSILON)
        << "LARGE brain sparsity should be " << EXPECTED_LARGE_SPARSITY;
}

/**
 * WHAT: Test sparsity for CUSTOM brain size
 * WHY:  Verify custom configuration defaults
 */
TEST(BrainInitConfig, Sparsity_Custom)
{
    float sparsity = nimcp_brain_factory_get_default_sparsity(BRAIN_SIZE_CUSTOM);
    EXPECT_NEAR(sparsity, EXPECTED_DEFAULT_SPARSITY, FLOAT_EPSILON)
        << "CUSTOM brain should default to " << EXPECTED_DEFAULT_SPARSITY << " sparsity";
}

/**
 * WHAT: Test sparsity ordering
 * WHY:  Verify larger networks have higher sparsity
 */
TEST(BrainInitConfig, Sparsity_Ordering)
{
    float micro = nimcp_brain_factory_get_default_sparsity(BRAIN_SIZE_MICRO);
    float tiny = nimcp_brain_factory_get_default_sparsity(BRAIN_SIZE_TINY);
    float small = nimcp_brain_factory_get_default_sparsity(BRAIN_SIZE_SMALL);
    float medium = nimcp_brain_factory_get_default_sparsity(BRAIN_SIZE_MEDIUM);
    float large = nimcp_brain_factory_get_default_sparsity(BRAIN_SIZE_LARGE);

    EXPECT_LT(micro, tiny) << "MICRO sparsity should be less than TINY";
    EXPECT_LT(tiny, small) << "TINY sparsity should be less than SMALL";
    EXPECT_LT(small, medium) << "SMALL sparsity should be less than MEDIUM";
    EXPECT_LT(medium, large) << "MEDIUM sparsity should be less than LARGE";
}

/**
 * WHAT: Test sparsity is in valid range
 * WHY:  Verify all sparsity values are between 0 and 1
 */
TEST(BrainInitConfig, Sparsity_ValidRange)
{
    float micro = nimcp_brain_factory_get_default_sparsity(BRAIN_SIZE_MICRO);
    float tiny = nimcp_brain_factory_get_default_sparsity(BRAIN_SIZE_TINY);
    float small = nimcp_brain_factory_get_default_sparsity(BRAIN_SIZE_SMALL);
    float medium = nimcp_brain_factory_get_default_sparsity(BRAIN_SIZE_MEDIUM);
    float large = nimcp_brain_factory_get_default_sparsity(BRAIN_SIZE_LARGE);

    EXPECT_GE(micro, 0.0f) << "Sparsity must be >= 0";
    EXPECT_LE(micro, 1.0f) << "Sparsity must be <= 1";

    EXPECT_GE(tiny, 0.0f) << "Sparsity must be >= 0";
    EXPECT_LE(tiny, 1.0f) << "Sparsity must be <= 1";

    EXPECT_GE(small, 0.0f);
    EXPECT_LE(small, 1.0f);

    EXPECT_GE(medium, 0.0f);
    EXPECT_LE(medium, 1.0f);

    EXPECT_GE(large, 0.0f);
    EXPECT_LE(large, 1.0f);
}

/**
 * WHAT: Test sparsity is reasonable for efficiency
 * WHY:  Verify sparsity values are in typical range (0.5-0.95)
 */
TEST(BrainInitConfig, Sparsity_ReasonableRange)
{
    float tiny = nimcp_brain_factory_get_default_sparsity(BRAIN_SIZE_TINY);
    float small = nimcp_brain_factory_get_default_sparsity(BRAIN_SIZE_SMALL);
    float medium = nimcp_brain_factory_get_default_sparsity(BRAIN_SIZE_MEDIUM);
    float large = nimcp_brain_factory_get_default_sparsity(BRAIN_SIZE_LARGE);

    EXPECT_GE(tiny, 0.5f) << "Sparsity should be at least 0.5 for efficiency";
    EXPECT_LE(large, 0.95f) << "Sparsity should be at most 0.95 to preserve connectivity";
}

/**
 * WHAT: Test sparsity with invalid enum
 * WHY:  Verify graceful handling of invalid values
 */
TEST(BrainInitConfig, Sparsity_InvalidEnum)
{
    brain_size_t invalid = static_cast<brain_size_t>(999);
    float sparsity = nimcp_brain_factory_get_default_sparsity(invalid);

    EXPECT_NEAR(sparsity, EXPECTED_DEFAULT_SPARSITY, FLOAT_EPSILON)
        << "Invalid size should return default sparsity";
}

/**
 * WHAT: Test sparsity consistency
 * WHY:  Verify repeated calls return same value
 */
TEST(BrainInitConfig, Sparsity_Consistency)
{
    for (int i = 0; i < 100; i++) {
        float sparsity = nimcp_brain_factory_get_default_sparsity(BRAIN_SIZE_MEDIUM);
        EXPECT_NEAR(sparsity, EXPECTED_MEDIUM_SPARSITY, FLOAT_EPSILON)
            << "Sparsity should be consistent across calls";
    }
}

/**
 * WHAT: Test sparsity is not NaN
 * WHY:  Verify numeric validity
 */
TEST(BrainInitConfig, Sparsity_NotNaN)
{
    EXPECT_FALSE(std::isnan(nimcp_brain_factory_get_default_sparsity(BRAIN_SIZE_MICRO)));
    EXPECT_FALSE(std::isnan(nimcp_brain_factory_get_default_sparsity(BRAIN_SIZE_TINY)));
    EXPECT_FALSE(std::isnan(nimcp_brain_factory_get_default_sparsity(BRAIN_SIZE_SMALL)));
    EXPECT_FALSE(std::isnan(nimcp_brain_factory_get_default_sparsity(BRAIN_SIZE_MEDIUM)));
    EXPECT_FALSE(std::isnan(nimcp_brain_factory_get_default_sparsity(BRAIN_SIZE_LARGE)));
}

/**
 * WHAT: Test sparsity is not infinity
 * WHY:  Verify numeric validity
 */
TEST(BrainInitConfig, Sparsity_NotInfinity)
{
    EXPECT_FALSE(std::isinf(nimcp_brain_factory_get_default_sparsity(BRAIN_SIZE_MICRO)));
    EXPECT_FALSE(std::isinf(nimcp_brain_factory_get_default_sparsity(BRAIN_SIZE_TINY)));
    EXPECT_FALSE(std::isinf(nimcp_brain_factory_get_default_sparsity(BRAIN_SIZE_SMALL)));
    EXPECT_FALSE(std::isinf(nimcp_brain_factory_get_default_sparsity(BRAIN_SIZE_MEDIUM)));
    EXPECT_FALSE(std::isinf(nimcp_brain_factory_get_default_sparsity(BRAIN_SIZE_LARGE)));
}

//=============================================================================
// Combined Configuration Tests
//=============================================================================

/**
 * WHAT: Test neuron count and sparsity correlation
 * WHY:  Verify larger networks have proportionally higher sparsity
 */
TEST(BrainInitConfig, NeuronCountSparsityCorrelation)
{
    struct SizeConfig {
        brain_size_t size;
        uint32_t neurons;
        float sparsity;
    };

    SizeConfig configs[] = {
        {BRAIN_SIZE_TINY, nimcp_brain_factory_get_neuron_count(BRAIN_SIZE_TINY),
         nimcp_brain_factory_get_default_sparsity(BRAIN_SIZE_TINY)},
        {BRAIN_SIZE_SMALL, nimcp_brain_factory_get_neuron_count(BRAIN_SIZE_SMALL),
         nimcp_brain_factory_get_default_sparsity(BRAIN_SIZE_SMALL)},
        {BRAIN_SIZE_MEDIUM, nimcp_brain_factory_get_neuron_count(BRAIN_SIZE_MEDIUM),
         nimcp_brain_factory_get_default_sparsity(BRAIN_SIZE_MEDIUM)},
        {BRAIN_SIZE_LARGE, nimcp_brain_factory_get_neuron_count(BRAIN_SIZE_LARGE),
         nimcp_brain_factory_get_default_sparsity(BRAIN_SIZE_LARGE)}
    };

    // Verify correlation: more neurons → higher sparsity
    for (size_t i = 1; i < 4; i++) {
        if (configs[i].neurons > configs[i-1].neurons) {
            EXPECT_GT(configs[i].sparsity, configs[i-1].sparsity)
                << "Larger networks should have higher sparsity for efficiency";
        }
    }
}

/**
 * WHAT: Test configuration consistency across all sizes
 * WHY:  Verify all size enums return valid configurations
 */
TEST(BrainInitConfig, AllSizesHaveValidConfig)
{
    brain_size_t sizes[] = {
        BRAIN_SIZE_MICRO,
        BRAIN_SIZE_TINY,
        BRAIN_SIZE_SMALL,
        BRAIN_SIZE_MEDIUM,
        BRAIN_SIZE_LARGE,
        BRAIN_SIZE_CUSTOM
    };

    for (brain_size_t size : sizes) {
        uint32_t neurons = nimcp_brain_factory_get_neuron_count(size);
        float sparsity = nimcp_brain_factory_get_default_sparsity(size);

        EXPECT_GT(neurons, 0u) << "Neuron count must be positive for size " << size;
        EXPECT_GE(sparsity, 0.0f) << "Sparsity must be non-negative for size " << size;
        EXPECT_LE(sparsity, 1.0f) << "Sparsity must be <= 1.0 for size " << size;
    }
}

/**
 * WHAT: Test memory implications of neuron count
 * WHY:  Verify configurations are memory-feasible
 */
TEST(BrainInitConfig, MemoryFeasibility)
{
    // Rough memory estimate: neuron count * 256 synapses * 4 bytes per weight
    const size_t BYTES_PER_SYNAPSE = 4;
    const size_t SYNAPSES_PER_NEURON = 256;
    const size_t MAX_REASONABLE_MEMORY = 10ULL * 1024 * 1024 * 1024;  // 10 GB

    brain_size_t sizes[] = {BRAIN_SIZE_TINY, BRAIN_SIZE_SMALL,
                            BRAIN_SIZE_MEDIUM, BRAIN_SIZE_LARGE};

    for (brain_size_t size : sizes) {
        uint32_t neurons = nimcp_brain_factory_get_neuron_count(size);
        size_t estimated_memory = static_cast<size_t>(neurons) *
                                 SYNAPSES_PER_NEURON * BYTES_PER_SYNAPSE;

        EXPECT_LT(estimated_memory, MAX_REASONABLE_MEMORY)
            << "Size " << size << " should use reasonable memory";
    }
}

/**
 * WHAT: Test default values match expected constants
 * WHY:  Ensure test constants are up to date
 */
TEST(BrainInitConfig, DefaultValuesMatchConstants)
{
    // This test documents the expected values
    EXPECT_EQ(nimcp_brain_factory_get_neuron_count(BRAIN_SIZE_MICRO), EXPECTED_MICRO_NEURONS);
    EXPECT_EQ(nimcp_brain_factory_get_neuron_count(BRAIN_SIZE_TINY), EXPECTED_TINY_NEURONS);
    EXPECT_EQ(nimcp_brain_factory_get_neuron_count(BRAIN_SIZE_SMALL), EXPECTED_SMALL_NEURONS);
    EXPECT_EQ(nimcp_brain_factory_get_neuron_count(BRAIN_SIZE_MEDIUM), EXPECTED_MEDIUM_NEURONS);
    EXPECT_EQ(nimcp_brain_factory_get_neuron_count(BRAIN_SIZE_LARGE), EXPECTED_LARGE_NEURONS);

    EXPECT_NEAR(nimcp_brain_factory_get_default_sparsity(BRAIN_SIZE_MICRO),
                EXPECTED_MICRO_SPARSITY, FLOAT_EPSILON);
    EXPECT_NEAR(nimcp_brain_factory_get_default_sparsity(BRAIN_SIZE_TINY),
                EXPECTED_TINY_SPARSITY, FLOAT_EPSILON);
    EXPECT_NEAR(nimcp_brain_factory_get_default_sparsity(BRAIN_SIZE_SMALL),
                EXPECTED_SMALL_SPARSITY, FLOAT_EPSILON);
    EXPECT_NEAR(nimcp_brain_factory_get_default_sparsity(BRAIN_SIZE_MEDIUM),
                EXPECTED_MEDIUM_SPARSITY, FLOAT_EPSILON);
    EXPECT_NEAR(nimcp_brain_factory_get_default_sparsity(BRAIN_SIZE_LARGE),
                EXPECTED_LARGE_SPARSITY, FLOAT_EPSILON);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
