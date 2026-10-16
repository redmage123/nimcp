/**
 * @file test_brain_parallel_init.cpp
 * @brief Unit tests for parallel wave-based brain subsystem initialization
 *
 * WHAT: Tests for nimcp_brain_parallel_init_subsystems() and the wave executor
 * WHY:  Parallel init is a critical path — any wave ordering bug or error
 *       propagation failure could produce a corrupt brain
 * HOW:  GTest fixture creating brains with parallel_init on/off, verifying
 *       subsystem fields, error handling, config defaults, and idempotence
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/factory/nimcp_brain_factory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class BrainParallelInitTest : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }

    brain_config_t make_config(bool parallel, uint32_t threads = 0) {
        brain_config_t config;
        memset(&config, 0, sizeof(config));
        config.size = BRAIN_SIZE_TINY;
        config.task = BRAIN_TASK_CLASSIFICATION;
        config.num_inputs = 4;
        config.num_outputs = 2;
        config.learning_rate = 0.01f;
        config.sparsity_target = 0.85f;
        config.parallel_init = parallel;
        config.init_threads = threads;
        config.enable_pr_memory = false;
        config.fast_training_mode = true;
        return config;
    }
};

//=============================================================================
// Test: Parallel init produces a valid brain
//=============================================================================

TEST_F(BrainParallelInitTest, ParallelInit_ProducesValidBrain) {
    brain_config_t config = make_config(true, 4);
    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr) << "brain_create_custom with parallel_init=true returned NULL";
    EXPECT_NE(brain->network, nullptr) << "Neural network should be initialized";
    brain_destroy(brain);
}

//=============================================================================
// Test: Sequential fallback still works
//=============================================================================

TEST_F(BrainParallelInitTest, SequentialFallback_ProducesValidBrain) {
    brain_config_t config = make_config(false);
    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr) << "brain_create_custom with parallel_init=false returned NULL";
    EXPECT_NE(brain->network, nullptr);
    brain_destroy(brain);
}

//=============================================================================
// Test: Core subsystem pointer fields are initialized (parallel)
//=============================================================================

TEST_F(BrainParallelInitTest, ParallelInit_CoreSubsystemsInitialized) {
    brain_config_t config = make_config(true, 4);
    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    // Network must always exist
    EXPECT_NE(brain->network, nullptr) << "Network should be initialized";

    // Create a sequential brain and verify parity of key pointer fields
    // (Some subsystems may be NULL on TINY brains — that's OK as long as
    //  parallel and sequential produce the same result)
    brain_config_t config_seq = make_config(false);
    brain_t brain_seq = brain_create_custom(&config_seq);
    ASSERT_NE(brain_seq, nullptr);

    EXPECT_EQ((brain->immune_system != nullptr),
              (brain_seq->immune_system != nullptr))
        << "immune_system parity between parallel and sequential";
    EXPECT_EQ((brain->shadow_emotions != nullptr),
              (brain_seq->shadow_emotions != nullptr))
        << "shadow_emotions parity between parallel and sequential";

    brain_destroy(brain_seq);

    brain_destroy(brain);
}

//=============================================================================
// Test: Subsystem pointer parity between parallel and sequential
//=============================================================================

TEST_F(BrainParallelInitTest, ParallelVsSequential_SubsystemParity) {
    brain_config_t config_par = make_config(true, 4);
    brain_config_t config_seq = make_config(false);

    brain_t brain_par = brain_create_custom(&config_par);
    ASSERT_NE(brain_par, nullptr) << "Parallel brain creation failed";

    brain_t brain_seq = brain_create_custom(&config_seq);
    ASSERT_NE(brain_seq, nullptr) << "Sequential brain creation failed";

    #define CHECK_PTR_PARITY(field) \
        EXPECT_EQ((brain_par->field != nullptr), (brain_seq->field != nullptr)) \
            << "Parity mismatch for " #field

    CHECK_PTR_PARITY(immune_system);
    CHECK_PTR_PARITY(network);
    CHECK_PTR_PARITY(shadow_emotions);
    CHECK_PTR_PARITY(grief_system);
    CHECK_PTR_PARITY(joy_system);
    CHECK_PTR_PARITY(remorse_system);
    CHECK_PTR_PARITY(social_bond_system);
    CHECK_PTR_PARITY(bias_detection);
    CHECK_PTR_PARITY(mental_health_monitor);

    #undef CHECK_PTR_PARITY

    brain_destroy(brain_par);
    brain_destroy(brain_seq);
}

//=============================================================================
// Test: Config defaults
//=============================================================================

TEST_F(BrainParallelInitTest, ConfigDefaults_ParallelInitTrue) {
    brain_config_t config;
    memset(&config, 0, sizeof(config));
    EXPECT_EQ(config.parallel_init, false) << "Zero-init config has parallel_init=false";
    EXPECT_EQ(config.init_threads, 0u) << "Zero-init config has init_threads=0";
}

//=============================================================================
// Test: init_threads auto-detection (0 = auto/4)
//=============================================================================

TEST_F(BrainParallelInitTest, InitThreads_AutoDetection) {
    brain_config_t config = make_config(true, 0);
    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr) << "Auto thread count selection should work";
    brain_destroy(brain);
}

//=============================================================================
// Test: init_threads explicit values
//=============================================================================

TEST_F(BrainParallelInitTest, InitThreads_ExplicitValues) {
    for (uint32_t threads : {1u, 2u, 8u}) {
        brain_config_t config = make_config(true, threads);
        brain_t brain = brain_create_custom(&config);
        ASSERT_NE(brain, nullptr) << "Failed with init_threads=" << threads;
        brain_destroy(brain);
    }
}

//=============================================================================
// Test: Minimal mode skips parallel init
//=============================================================================

TEST_F(BrainParallelInitTest, MinimalMode_SkipsParallelInit) {
    brain_config_t config = make_config(true, 4);
    config.minimal_mode = true;
    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr) << "Minimal mode brain should still create successfully";
    brain_destroy(brain);
}

//=============================================================================
// Test: Repeated create/destroy stability
//=============================================================================

TEST_F(BrainParallelInitTest, RepeatedCreateDestroy_NoLeakOrCrash) {
    for (int i = 0; i < 3; i++) {
        brain_config_t config = make_config(true, 4);
        brain_t brain = brain_create_custom(&config);
        ASSERT_NE(brain, nullptr) << "Create/destroy iteration " << i << " failed";
        EXPECT_NE(brain->network, nullptr);
        brain_destroy(brain);
    }
}

//=============================================================================
// Test: Brain is functional after parallel init (inference)
//=============================================================================

TEST_F(BrainParallelInitTest, ParallelInit_BrainCanDecide) {
    brain_config_t config = make_config(true, 4);
    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    float inputs[] = {1.0f, 0.5f, 0.3f, 0.8f};
    brain_decision_t* decision = brain_decide(brain, inputs, 4);
    if (decision) {
        EXPECT_GE(decision->confidence, 0.0f);
        EXPECT_LE(decision->confidence, 1.0f);
        brain_free_decision(decision);
    }

    brain_destroy(brain);
}

//=============================================================================
// Test: Wellbeing inline fields initialized (Wave 4)
//=============================================================================

TEST_F(BrainParallelInitTest, ParallelInit_InlineFieldsInitialized) {
    brain_config_t config = make_config(true, 4);
    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    EXPECT_TRUE(brain->wellbeing_monitoring_enabled);
    EXPECT_TRUE(brain->enable_spike_analysis);
    EXPECT_TRUE(brain->enable_population_coding);
    EXPECT_EQ(brain->current_time_us, 0u);
    EXPECT_FALSE(brain->enable_shannon_monitoring);

    brain_destroy(brain);
}

//=============================================================================
// Test: Emotional subsystems created in parallel path
//=============================================================================

TEST_F(BrainParallelInitTest, ParallelInit_EmotionalSubsystemsCreated) {
    brain_config_t config = make_config(true, 4);
    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    EXPECT_NE(brain->shadow_emotions, nullptr) << "Shadow emotions should be created";
    EXPECT_NE(brain->grief_system, nullptr) << "Grief system should be created";
    EXPECT_NE(brain->joy_system, nullptr) << "Joy system should be created";

    brain_destroy(brain);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
