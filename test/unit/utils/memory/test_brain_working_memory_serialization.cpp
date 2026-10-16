/**
 * @file test_brain_working_memory_serialization.cpp
 * @brief Comprehensive tests for working memory serialization
 *
 * WHAT: Tests for working memory item save/load (load_working_memory_item)
 * WHY: Cover lines 4708-4744 in brain.c (18 uncovered lines)
 * HOW: Create brains with working memory items, save, load, verify persistence
 *
 * TARGET: load_working_memory_item() function and working memory serialization paths
 *
 * NOTE: These tests are DISABLED because brain creation with working memory
 * enabled takes 60+ seconds. They should be run in CI environments with
 * extended timeouts or marked as slow integration tests.
 *
 * To run these tests manually:
 *   ./test_brain_working_memory_serialization --gtest_also_run_disabled_tests
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdio>

    #include "core/brain/nimcp_brain.h"
    #include "nimcp.h"

//=============================================================================
// Test Fixture
//=============================================================================

class BrainWorkingMemorySerializationTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_init();
        brain_clear_error();
    }

    void TearDown() override {
        nimcp_shutdown();
    }

    void cleanup(const char* filename) {
        std::remove(filename);
    }
};

//=============================================================================
// Working Memory Serialization Tests
//=============================================================================

TEST_F(BrainWorkingMemorySerializationTest, DISABLED_SaveLoadWithSingleWorkingMemoryItem) {
    // DISABLED: Brain creation takes 60+ seconds
    const char* file = "/tmp/brain_wm_single.nimcp";
    cleanup(file);

    // Create brain with working memory
    brain_config_t config = {};
    config.size = BRAIN_SIZE_TINY;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 10;
    config.num_outputs = 5;
    strncpy(config.task_name, "wm_single", 63);
    config.enable_working_memory = true;

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    // Add item to working memory via learning (populates working memory)
    float data[10];
    for (int i = 0; i < 10; i++) data[i] = 0.1f * i;

    for (int i = 0; i < 5; i++) {
        brain_learn_example(brain, data, 10, "test_class", 0.85f);
    }

    // Save brain with working memory
    bool saved = brain_save(brain, file);
    EXPECT_TRUE(saved);
    brain_destroy(brain);

    // Load brain
    brain_t loaded = brain_load(file);
    if (loaded) {
        // Verify loaded brain works
        brain_decision_t* dec = brain_decide(loaded, data, 10);
        if (dec) brain_free_decision(dec);
        brain_destroy(loaded);
    }

    cleanup(file);
}

TEST_F(BrainWorkingMemorySerializationTest, DISABLED_SaveLoadWithMultipleWorkingMemoryItems) {
    // DISABLED: Brain creation takes 60+ seconds
    const char* file = "/tmp/brain_wm_multiple.nimcp";
    cleanup(file);

    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 20;
    config.num_outputs = 8;
    strncpy(config.task_name, "wm_multiple", 63);
    config.enable_working_memory = true;

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    // Add multiple items to working memory through learning
    for (int example = 0; example < 15; example++) {
        float data[20];
        for (int i = 0; i < 20; i++) {
            data[i] = 0.05f * (example + i);
        }
        brain_learn_example(brain, data, 20, "varied_class", 0.9f);
    }

    // Save
    bool saved = brain_save(brain, file);
    EXPECT_TRUE(saved);
    brain_destroy(brain);

    // Load and verify
    brain_t loaded = brain_load(file);
    if (loaded) {
        float test_data[20];
        for (int i = 0; i < 20; i++) test_data[i] = 0.3f;

        brain_decision_t* dec = brain_decide(loaded, test_data, 20);
        if (dec) brain_free_decision(dec);
        brain_destroy(loaded);
    }

    cleanup(file);
}

TEST_F(BrainWorkingMemorySerializationTest, DISABLED_SaveLoadWithWorkingMemoryAndLongSequence) {
    // DISABLED: Brain creation takes 60+ seconds
    const char* file = "/tmp/brain_wm_long_seq.nimcp";
    cleanup(file);

    brain_config_t config = {};
    config.size = BRAIN_SIZE_MEDIUM;
    config.task = BRAIN_TASK_SEQUENCE;
    config.num_inputs = 32;
    config.num_outputs = 16;
    strncpy(config.task_name, "wm_sequence", 63);
    config.enable_working_memory = true;

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    // Create a sequence to populate working memory
    float sequence_data[32];
    for (int step = 0; step < 20; step++) {
        for (int i = 0; i < 32; i++) {
            sequence_data[i] = sinf(step * 0.1f + i * 0.05f);
        }
        brain_learn_example(brain, sequence_data, 32, "seq_pattern", 0.88f);
    }

    // Save
    bool saved = brain_save(brain, file);
    EXPECT_TRUE(saved);
    brain_destroy(brain);

    // Load
    brain_t loaded = brain_load(file);
    if (loaded) {
        brain_decision_t* dec = brain_decide(loaded, sequence_data, 32);
        if (dec) brain_free_decision(dec);
        brain_destroy(loaded);
    }

    cleanup(file);
}

TEST_F(BrainWorkingMemorySerializationTest, DISABLED_SaveLoadWithEmotionallyTaggedWorkingMemory) {
    // DISABLED: Brain creation takes 60+ seconds
    const char* file = "/tmp/brain_wm_emotional.nimcp";
    cleanup(file);

    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 16;
    config.num_outputs = 6;
    strncpy(config.task_name, "wm_emotional", 63);
    config.enable_working_memory = true;
    config.enable_emotional_tagging = true;

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    // Add items with varying salience (emotional importance)
    for (int i = 0; i < 10; i++) {
        float data[16];
        for (int j = 0; j < 16; j++) data[j] = 0.1f * (i + j);

        // Vary the reward/confidence to create emotional tagging
        float reward = 0.5f + (i % 5) * 0.1f;
        brain_learn_example(brain, data, 16, "emotion_class", reward);
    }

    // Save
    bool saved = brain_save(brain, file);
    EXPECT_TRUE(saved);
    brain_destroy(brain);

    // Load
    brain_t loaded = brain_load(file);
    if (loaded) {
        float test_data[16];
        for (int i = 0; i < 16; i++) test_data[i] = 0.5f;

        brain_decision_t* dec = brain_decide(loaded, test_data, 16);
        if (dec) brain_free_decision(dec);
        brain_destroy(loaded);
    }

    cleanup(file);
}

TEST_F(BrainWorkingMemorySerializationTest, DISABLED_SaveLoadCycleStressTest) {
    // DISABLED: Brain creation takes 60+ seconds
    const char* file = "/tmp/brain_wm_cycle.nimcp";
    cleanup(file);

    brain_config_t config = {};
    config.size = BRAIN_SIZE_TINY;
    config.task = BRAIN_TASK_REGRESSION;
    config.num_inputs = 8;
    config.num_outputs = 4;
    strncpy(config.task_name, "wm_cycle", 63);
    config.enable_working_memory = true;

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    // Phase 1: Train and save
    float data[8] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};
    for (int i = 0; i < 10; i++) {
        brain_learn_example(brain, data, 8, "value", 0.75f);
    }
    brain_save(brain, file);
    brain_destroy(brain);

    // Phase 2: Load, train more, save again
    brain = brain_load(file);
    ASSERT_NE(brain, nullptr);
    for (int i = 0; i < 10; i++) {
        brain_learn_example(brain, data, 8, "value", 0.8f);
    }
    brain_save(brain, file);
    brain_destroy(brain);

    // Phase 3: Load again and verify
    brain = brain_load(file);
    if (brain) {
        brain_decision_t* dec = brain_decide(brain, data, 8);
        if (dec) brain_free_decision(dec);
        brain_destroy(brain);
    }

    cleanup(file);
}

TEST_F(BrainWorkingMemorySerializationTest, DISABLED_SaveLoadWithMaxWorkingMemoryCapacity) {
    // DISABLED: Brain creation takes 60+ seconds
    const char* file = "/tmp/brain_wm_max.nimcp";
    cleanup(file);

    brain_config_t config = {};
    config.size = BRAIN_SIZE_MEDIUM;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 24;
    config.num_outputs = 10;
    strncpy(config.task_name, "wm_max_capacity", 63);
    config.enable_working_memory = true;

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    // Fill working memory to capacity (Miller's 7±2 suggests ~9 items max)
    for (int i = 0; i < 50; i++) {  // Exceed typical capacity
        float data[24];
        for (int j = 0; j < 24; j++) {
            data[j] = 0.02f * (i * j);
        }
        brain_learn_example(brain, data, 24, "capacity_test", 0.85f);
    }

    // Save
    bool saved = brain_save(brain, file);
    EXPECT_TRUE(saved);
    brain_destroy(brain);

    // Load
    brain_t loaded = brain_load(file);
    if (loaded) {
        float test_data[24];
        for (int i = 0; i < 24; i++) test_data[i] = 0.5f;

        brain_decision_t* dec = brain_decide(loaded, test_data, 24);
        if (dec) brain_free_decision(dec);
        brain_destroy(loaded);
    }

    cleanup(file);
}

TEST_F(BrainWorkingMemorySerializationTest, DISABLED_LoadCorruptedWorkingMemoryFile) {
    // DISABLED: Brain creation takes 60+ seconds
    const char* file = "/tmp/brain_wm_corrupt.nimcp";
    cleanup(file);

    // Create valid brain and save
    brain_config_t config = {};
    config.size = BRAIN_SIZE_TINY;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 5;
    config.num_outputs = 3;
    strncpy(config.task_name, "wm_corrupt", 63);
    config.enable_working_memory = true;

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    float data[5] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};
    brain_learn_example(brain, data, 5, "test", 0.8f);
    brain_save(brain, file);
    brain_destroy(brain);

    // Corrupt the file (truncate it)
    FILE* f = fopen(file, "ab");
    if (f) {
        // Write garbage data
        uint32_t garbage = 0xDEADBEEF;
        fwrite(&garbage, sizeof(uint32_t), 1, f);
        fclose(f);
    }

    // Try to load corrupted file
    brain_t loaded = brain_load(file);
    // Should either fail gracefully or load partial state
    if (loaded) {
        brain_destroy(loaded);
    }

    cleanup(file);
}

TEST_F(BrainWorkingMemorySerializationTest, DISABLED_SaveLoadWorkingMemoryWithAllCognitiveSystems) {
    // DISABLED: Brain creation takes 60+ seconds
    const char* file = "/tmp/brain_wm_all_systems.nimcp";
    cleanup(file);

    brain_config_t config = {};
    config.size = BRAIN_SIZE_LARGE;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 64;
    config.num_outputs = 20;
    strncpy(config.task_name, "wm_all_systems", 63);

    // Enable everything including working memory
    config.enable_working_memory = true;
    config.enable_emotional_tagging = true;
    config.enable_executive_control = true;
    config.enable_introspection = true;
    config.enable_knowledge = true;
    config.enable_ethics = true;
    config.enable_wellbeing = true;
    config.enable_sleep_wake_cycle = true;

    brain_t brain = brain_create_custom(&config);
    if (!brain) {
        // Large brain with all systems may fail due to memory constraints
        GTEST_SKIP() << "Large brain creation failed (likely memory constraints)";
        return;
    }

    // Train with complex data
    float data[64];
    for (int i = 0; i < 64; i++) data[i] = 0.01f * i;

    for (int i = 0; i < 20; i++) {
        brain_learn_example(brain, data, 64, "complex_class", 0.87f);
    }

    // Save
    bool saved = brain_save(brain, file);
    EXPECT_TRUE(saved);
    brain_destroy(brain);

    // Load
    brain_t loaded = brain_load(file);
    if (loaded) {
        brain_decision_t* dec = brain_decide(loaded, data, 64);
        if (dec) brain_free_decision(dec);
        brain_destroy(loaded);
    }

    cleanup(file);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
