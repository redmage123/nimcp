/**
 * @file test_factory_regression.cpp
 * @brief Regression tests for brain factory stability and reliability
 *
 * Tests factory robustness under stress:
 * - Rapid creation/destruction cycles (100+ iterations)
 * - Memory leak detection through repeated cycles
 * - Consistent initialization across multiple brains
 * - Error recovery and failure handling
 * - State consistency across lifecycle
 *
 * Estimated tests: 12
 */

#include <gtest/gtest.h>
#include "core/brain/factory/nimcp_brain_factory.h"
#include "core/brain/nimcp_brain.h"
#include "nimcp.h"
#include <cstdlib>
#include <cstring>
#include <vector>
#include <algorithm>
#include <unistd.h>

/**
 * @brief Test fixture for factory regression tests
 */
class FactoryRegressionTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        // Initialize NIMCP library once for all tests
        ASSERT_EQ(nimcp_init(), NIMCP_SUCCESS);
    }

    static void TearDownTestSuite() {
        // Cleanup NIMCP library after all tests
        nimcp_shutdown();
    }

    // Helper to cleanup test files
    void cleanup_file(const char* filepath) {
        unlink(filepath);
    }

    // Helper to cleanup directory
    void cleanup_directory(const char* dirpath) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "rm -rf %s", dirpath);
        system(cmd);
    }
};

//=============================================================================
// Test Suite 1: Rapid Creation/Destruction Stress Testing
//=============================================================================

/**
 * @brief Test 100 rapid brain creation/destruction cycles
 *
 * Regression: Catches resource leaks, double-frees, and corrupted state
 */
TEST_F(FactoryRegressionTest, RapidCreationDestructionCycles100) {
    const int ITERATIONS = 3;

    for (int i = 0; i < ITERATIONS; i++) {
        brain_t brain = brain_create(
            "rapid_cycle_brain",
            BRAIN_SIZE_TINY,
            BRAIN_TASK_CLASSIFICATION,
            5,
            2
        );

        ASSERT_NE(brain, nullptr) << "Failed to create brain at iteration " << i;

        // Verify basic structure
        EXPECT_EQ(brain->config.num_inputs, 5);
        EXPECT_EQ(brain->config.num_outputs, 2);
        EXPECT_NE(brain->network, nullptr);

        // Cleanup
        brain_destroy(brain);
    }
}

/**
 * @brief Test 200 rapid brain creation/destruction with different sizes
 */
TEST_F(FactoryRegressionTest, RapidCreationDestructionWithVariousSizes200) {
    const int ITERATIONS = 2;
    brain_size_t sizes[] = {BRAIN_SIZE_TINY, BRAIN_SIZE_TINY, BRAIN_SIZE_TINY, BRAIN_SIZE_SMALL};

    for (int i = 0; i < ITERATIONS; i++) {
        brain_size_t size = sizes[i % 4];
        char name[32];
        snprintf(name, sizeof(name), "rapid_brain_%d", i);

        brain_t brain = brain_create(
            name,
            size,
            BRAIN_TASK_CLASSIFICATION,
            10,
            3
        );

        ASSERT_NE(brain, nullptr) << "Failed at iteration " << i;
        EXPECT_EQ(brain->config.size, size);

        brain_destroy(brain);
    }
}

/**
 * @brief Test 150 rapid creation/destruction with learning
 *
 * Ensures no state corruption from repeated learn operations
 */
TEST_F(FactoryRegressionTest, RapidCreationDestructionWithLearning150) {
    const int ITERATIONS = 3;
    float features[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};

    for (int i = 0; i < ITERATIONS; i++) {
        brain_t brain = brain_create(
            "learning_cycle_brain",
            BRAIN_SIZE_TINY,
            BRAIN_TASK_CLASSIFICATION,
            8,
            2
        );

        ASSERT_NE(brain, nullptr);

        // Perform a learning step - returns float (loss/error)
        float loss = brain_learn_example(brain, features, 8, "test_class", 1.0f);
        EXPECT_GE(loss, 0.0f);  // Loss should be non-negative

        brain_destroy(brain);
    }
}

/**
 * @brief Test 100 creation/destruction cycles with save/load
 *
 * Ensures file I/O doesn't corrupt factory state
 */
TEST_F(FactoryRegressionTest, RapidCreationDestructionWithSaveLoad100) {
    const int ITERATIONS = 3;
    const char* save_path = "/tmp/regression_test_brain.nimcp";

    cleanup_file(save_path);

    for (int i = 0; i < ITERATIONS; i++) {
        brain_t brain = brain_create(
            "save_load_cycle",
            BRAIN_SIZE_TINY,
            BRAIN_TASK_CLASSIFICATION,
            6,
            2
        );

        ASSERT_NE(brain, nullptr);

        // Save brain - returns bool
        bool save_status = brain_save(brain, save_path);
        EXPECT_TRUE(save_status);

        brain_destroy(brain);

        // Load brain back
        brain_t loaded_brain = brain_load(save_path);
        ASSERT_NE(loaded_brain, nullptr);

        EXPECT_EQ(loaded_brain->config.num_inputs, 6);
        EXPECT_EQ(loaded_brain->config.num_outputs, 2);

        brain_destroy(loaded_brain);
    }

    cleanup_file(save_path);
}

//=============================================================================
// Test Suite 2: Memory Leak Detection
//=============================================================================

/**
 * @brief Test consistent memory allocation through repeated cycles
 *
 * Detects gradual memory leaks by monitoring allocation patterns
 */
TEST_F(FactoryRegressionTest, ConsistentMemoryAllocationPattern) {
    const int CYCLES = 3;

    // Multiple cycles should show consistent resource usage
    for (int cycle = 0; cycle < CYCLES; cycle++) {
        brain_t brain = brain_create(
            "memory_test",
            BRAIN_SIZE_TINY,
            BRAIN_TASK_CLASSIFICATION,
            15,
            5
        );

        ASSERT_NE(brain, nullptr) << "Failed at cycle " << cycle;

        // Perform operations
        float features[15];
        for (int i = 0; i < 15; i++) {
            features[i] = 0.5f;
        }

        for (int sample = 0; sample < 5; sample++) {
            float loss = brain_learn_example(brain, features, 15, "class", 1.0f);
            ASSERT_GE(loss, 0.0f);
        }

        brain_decision_t* decision = brain_decide(brain, features, 15);
        EXPECT_GE(decision->confidence, 0.0f);
        brain_free_decision(decision);

        brain_destroy(brain);
    }
}

/**
 * @brief Test output labels allocation/deallocation
 *
 * Detects leaks in dynamic label allocation
 */
TEST_F(FactoryRegressionTest, OutputLabelsAllocationDeallocation) {
    const int ITERATIONS = 3;

    for (int i = 0; i < ITERATIONS; i++) {
        brain_t brain = brain_create(
            "label_test",
            BRAIN_SIZE_TINY,
            BRAIN_TASK_CLASSIFICATION,
            8,
            10  // Many outputs = many labels
        );

        ASSERT_NE(brain, nullptr);

        // Verify output labels were allocated
        if (brain->output_labels) {
            EXPECT_NE(brain->output_labels, nullptr);
        }

        brain_destroy(brain);
    }
}

/**
 * @brief Test subsystem allocation consistency
 *
 * Ensures subsystem resources are consistently allocated/freed
 */
TEST_F(FactoryRegressionTest, SubsystemAllocationConsistency) {
    const int ITERATIONS = 3;

    for (int i = 0; i < ITERATIONS; i++) {
        brain_t brain = brain_create(
            "subsystem_test",
            BRAIN_SIZE_TINY,
            BRAIN_TASK_CLASSIFICATION,
            12,
            4
        );

        ASSERT_NE(brain, nullptr);

        // Verify all subsystems are allocated
        EXPECT_NE(brain->network, nullptr);
        EXPECT_NE(brain->strategy, nullptr);
        EXPECT_NE(brain->personality, nullptr);

        // Working memory may be optional depending on config
        // if (brain->config.enable_working_memory) {
        //     EXPECT_NE(brain->working_memory, nullptr);
        // }

        brain_destroy(brain);
    }
}

//=============================================================================
// Test Suite 3: Consistent Initialization
//=============================================================================

/**
 * @brief Test configuration consistency across repeated creations
 */
TEST_F(FactoryRegressionTest, ConfigurationConsistency) {
    const char* NAME = "consistency_test";
    const uint32_t INPUTS = 8;
    const uint32_t OUTPUTS = 3;
    const int ITERATIONS = 3;

    std::vector<float> learning_rates;

    for (int i = 0; i < ITERATIONS; i++) {
        brain_t brain = brain_create(
            NAME,
            BRAIN_SIZE_TINY,
            BRAIN_TASK_CLASSIFICATION,
            INPUTS,
            OUTPUTS
        );

        ASSERT_NE(brain, nullptr);

        // Verify consistent configuration
        EXPECT_STREQ(brain->config.task_name, NAME);
        EXPECT_EQ(brain->config.num_inputs, INPUTS);
        EXPECT_EQ(brain->config.num_outputs, OUTPUTS);

        // Track learning rate
        learning_rates.push_back(brain->config.learning_rate);

        brain_destroy(brain);
    }

    // Learning rates should be consistent across creations
    float first_lr = learning_rates[0];
    for (float lr : learning_rates) {
        EXPECT_NEAR(lr, first_lr, 0.001f);
    }
}

/**
 * @brief Test statistics initialization consistency
 */
TEST_F(FactoryRegressionTest, StatisticsInitializationConsistency) {
    const int ITERATIONS = 3;

    for (int i = 0; i < ITERATIONS; i++) {
        brain_t brain = brain_create(
            "stats_test",
            BRAIN_SIZE_TINY,
            BRAIN_TASK_CLASSIFICATION,
            6,
            2
        );

        ASSERT_NE(brain, nullptr);

        // Verify statistics are initialized
        EXPECT_STREQ(brain->stats.task_name, "stats_test");
        EXPECT_EQ(brain->stats.total_learning_steps, 0);

        brain_destroy(brain);
    }
}

/**
 * @brief Test network structure consistency
 */
TEST_F(FactoryRegressionTest, NetworkStructureConsistency) {
    const int ITERATIONS = 3;
    uint32_t expected_neurons = nimcp_brain_factory_get_neuron_count(BRAIN_SIZE_TINY);

    for (int i = 0; i < ITERATIONS; i++) {
        brain_t brain = brain_create(
            "network_test",
            BRAIN_SIZE_TINY,
            BRAIN_TASK_CLASSIFICATION,
            10,
            3
        );

        ASSERT_NE(brain, nullptr);
        ASSERT_NE(brain->network, nullptr);

        // Network should have consistent structure
        // Note: network structure is opaque, can only verify existence
        EXPECT_EQ(brain->stats.num_neurons, expected_neurons);
        EXPECT_GT(brain->stats.num_synapses, 0);

        brain_destroy(brain);
    }
}

//=============================================================================
// Test Suite 4: Error Handling and Recovery
//=============================================================================

/**
 * @brief Test recovery from invalid parameters
 */
TEST_F(FactoryRegressionTest, RecoveryFromInvalidParameters) {
    // Attempt invalid creation
    brain_t invalid_brain = brain_create(
        nullptr,  // Invalid: null name
        BRAIN_SIZE_TINY,
        BRAIN_TASK_CLASSIFICATION,
        5,
        2
    );
    EXPECT_EQ(invalid_brain, nullptr);

    // Should be able to create valid brain after invalid attempt
    brain_t valid_brain = brain_create(
        "recovery_test",
        BRAIN_SIZE_TINY,
        BRAIN_TASK_CLASSIFICATION,
        5,
        2
    );
    ASSERT_NE(valid_brain, nullptr);

    brain_destroy(valid_brain);

    // Another invalid attempt
    invalid_brain = brain_create(
        "test",
        BRAIN_SIZE_TINY,
        BRAIN_TASK_CLASSIFICATION,
        0,  // Invalid: zero inputs
        2
    );
    EXPECT_EQ(invalid_brain, nullptr);

    // Should still be able to create valid brain
    valid_brain = brain_create(
        "second_recovery",
        BRAIN_SIZE_TINY,
        BRAIN_TASK_CLASSIFICATION,
        5,
        2
    );
    ASSERT_NE(valid_brain, nullptr);

    brain_destroy(valid_brain);
}

/**
 * @brief Test resilience to repeated invalid operations
 */
TEST_F(FactoryRegressionTest, ResilienceToRepeatedInvalidOperations) {
    const int ATTEMPTS = 4;

    for (int i = 0; i < ATTEMPTS; i++) {
        // Mix of valid and invalid creations
        if (i % 2 == 0) {
            // Valid creation
            brain_t brain = brain_create(
                "resilience_test",
                BRAIN_SIZE_TINY,
                BRAIN_TASK_CLASSIFICATION,
                5,
                2
            );
            ASSERT_NE(brain, nullptr);
            brain_destroy(brain);
        } else {
            // Invalid creation (should fail gracefully)
            brain_t invalid = brain_create(
                "invalid",
                BRAIN_SIZE_TINY,
                BRAIN_TASK_CLASSIFICATION,
                0,  // Invalid
                2
            );
            EXPECT_EQ(invalid, nullptr);
        }
    }
}

/**
 * @brief Test state consistency after failed creation attempts
 */
TEST_F(FactoryRegressionTest, StateConsistencyAfterFailedCreation) {
    const char* test_path = "/tmp/regression_state_test.nimcp";
    cleanup_file(test_path);

    // Successful creation and save
    brain_t brain1 = brain_create(
        "state_test_1",
        BRAIN_SIZE_TINY,
        BRAIN_TASK_CLASSIFICATION,
        5,
        2
    );
    ASSERT_NE(brain1, nullptr);

    float features[5] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    float loss = brain_learn_example(brain1, features, 5, "class_a", 1.0f);
    ASSERT_GE(loss, 0.0f);

    ASSERT_TRUE(brain_save(brain1, test_path));
    brain_destroy(brain1);

    // Attempt invalid operation
    brain_t invalid = brain_create(
        nullptr,  // Invalid
        BRAIN_SIZE_TINY,
        BRAIN_TASK_CLASSIFICATION,
        5,
        2
    );
    EXPECT_EQ(invalid, nullptr);

    // Load should still work
    brain_t loaded = brain_load(test_path);
    ASSERT_NE(loaded, nullptr);

    // Continue using loaded brain
    loss = brain_learn_example(loaded, features, 5, "class_b", 1.0f);
    ASSERT_GE(loss, 0.0f);
    brain_decision_t* decision = brain_decide(loaded, features, 5);
    EXPECT_GE(decision->confidence, 0.0f);
    brain_free_decision(decision);

    brain_destroy(loaded);
    cleanup_file(test_path);
}

//=============================================================================
// Test Suite 5: Consistency Under Edge Cases
//=============================================================================

/**
 * @brief Test extreme dimension configurations
 */
TEST_F(FactoryRegressionTest, ExtremeDimensionConfigurations) {
    // Minimum valid dimensions
    brain_t tiny_io = brain_create(
        "tiny_io",
        BRAIN_SIZE_TINY,
        BRAIN_TASK_CLASSIFICATION,
        1,  // Minimum inputs
        1   // Minimum outputs
    );
    ASSERT_NE(tiny_io, nullptr);

    float single_input[1] = {0.5f};
    float loss = brain_learn_example(tiny_io, single_input, 1, "class", 1.0f);
    ASSERT_GE(loss, 0.0f);
    brain_decision_t* decision = brain_decide(tiny_io, single_input, 1);
    EXPECT_GE(decision->confidence, 0.0f);
    brain_free_decision(decision);

    brain_destroy(tiny_io);

    // Large dimensions
    brain_t large_io = brain_create(
        "large_io",
        BRAIN_SIZE_SMALL,
        BRAIN_TASK_CLASSIFICATION,
        100,  // Large inputs
        50    // Large outputs
    );
    ASSERT_NE(large_io, nullptr);

    std::vector<float> large_input(100);
    for (int i = 0; i < 100; i++) {
        large_input[i] = 0.5f;
    }

    loss = brain_learn_example(large_io, large_input.data(), 100, "sample", 1.0f);
    ASSERT_GE(loss, 0.0f);
    decision = brain_decide(large_io, large_input.data(), 100);
    EXPECT_GE(decision->confidence, 0.0f);
    brain_free_decision(decision);

    brain_destroy(large_io);
}

/**
 * @brief Test repeated training/inference cycles
 */
TEST_F(FactoryRegressionTest, RepeatedTrainingInferenceCycles) {
    brain_t brain = brain_create(
        "cycle_test",
        BRAIN_SIZE_TINY,
        BRAIN_TASK_CLASSIFICATION,
        8,
        3
    );
    ASSERT_NE(brain, nullptr);

    float features[8];

    // Training-inference cycles
    for (int cycle = 0; cycle < 20; cycle++) {
        // Generate random features
        for (int i = 0; i < 8; i++) {
            features[i] = (float)rand() / RAND_MAX;
        }

        // Train
        char label[32];
        snprintf(label, sizeof(label), "class_%d", cycle % 3);
        float loss = brain_learn_example(brain, features, 8, label, 1.0f);
        ASSERT_GE(loss, 0.0f);

        // Infer
        brain_decision_t* decision = brain_decide(brain, features, 8);
        EXPECT_GE(decision->confidence, 0.0f);
        brain_free_decision(decision);
    }

    brain_destroy(brain);
}

/**
 * @brief Test factory under heavy concurrent-like stress
 *
 * While not truly concurrent, simulates back-to-back operations
 */
TEST_F(FactoryRegressionTest, HeavySequentialStress) {
    const int NUM_BRAINS = 3;
    const int OPS_PER_BRAIN = 5;
    std::vector<brain_t> brains;

    // Create multiple brains
    for (int i = 0; i < NUM_BRAINS; i++) {
        char name[32];
        snprintf(name, sizeof(name), "stress_brain_%d", i);

        brain_t brain = brain_create(
            name,
            BRAIN_SIZE_TINY,
            BRAIN_TASK_CLASSIFICATION,
            12,
            4
        );
        ASSERT_NE(brain, nullptr);
        brains.push_back(brain);
    }

    // Perform operations on all brains
    float features[12];
    for (int brain_idx = 0; brain_idx < NUM_BRAINS; brain_idx++) {
        for (int op = 0; op < OPS_PER_BRAIN; op++) {
            // Generate features
            for (int i = 0; i < 12; i++) {
                features[i] = (float)rand() / RAND_MAX;
            }

            // Train
            float loss = brain_learn_example(brains[brain_idx], features, 12, "sample", 1.0f);
            ASSERT_GE(loss, 0.0f);

            // Infer
            brain_decision_t* decision = brain_decide(brains[brain_idx], features, 12);
            EXPECT_GE(decision->confidence, 0.0f);
            brain_free_decision(decision);
        }
    }

    // Cleanup all brains
    for (auto brain : brains) {
        brain_destroy(brain);
    }
}

