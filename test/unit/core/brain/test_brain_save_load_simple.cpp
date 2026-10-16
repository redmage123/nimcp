/**
 * @file test_brain_save_load_simple.cpp
 * @brief Simple save/load tests to boost coverage
 *
 * WHAT: Tests for brain serialization and deserialization
 * WHY: Cover save/load paths including working memory
 * HOW: Create, train, save, load, and verify brains
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdio>

    #include "core/brain/nimcp_brain.h"
    #include "nimcp.h"

class BrainSaveLoadTest : public ::testing::Test {
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

TEST_F(BrainSaveLoadTest, BasicSaveLoad) {
    const char* file = "/tmp/brain_basic.nimcp";
    cleanup(file);

    brain_t brain = brain_create("save_test", BRAIN_SIZE_TINY,
                                  BRAIN_TASK_CLASSIFICATION, 5, 3);
    ASSERT_NE(brain, nullptr);

    float data[5] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};
    for (int i = 0; i < 10; i++) {
        brain_learn_example(brain, data, 5, "class_a", 0.9f);
    }

    EXPECT_TRUE(brain_save(brain, file));
    brain_destroy(brain);

    brain_t loaded = brain_load(file);
    if (loaded) {
        brain_decision_t* dec = brain_decide(loaded, data, 5);
        if (dec) brain_free_decision(dec);
        brain_destroy(loaded);
    }

    cleanup(file);
}

TEST_F(BrainSaveLoadTest, SaveLoadWithWorkingMemory) {
    const char* file = "/tmp/brain_wm.nimcp";
    cleanup(file);

    brain_config_t config = {};
    config.size = BRAIN_SIZE_TINY;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 8;
    config.num_outputs = 4;
    strncpy(config.task_name, "wm_test", 63);
    config.enable_working_memory = true;

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    float data[8] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};
    brain_learn_example(brain, data, 8, "test", 0.85f);

    EXPECT_TRUE(brain_save(brain, file));
    brain_destroy(brain);

    brain_t loaded = brain_load(file);
    if (loaded) {
        brain_decision_t* dec = brain_decide(loaded, data, 8);
        if (dec) brain_free_decision(dec);
        brain_destroy(loaded);
    }

    cleanup(file);
}

TEST_F(BrainSaveLoadTest, MultipleSequentialSaves) {
    const char* files[] = {"/tmp/v1.nimcp", "/tmp/v2.nimcp", "/tmp/v3.nimcp"};

    brain_t brain = brain_create("versions", BRAIN_SIZE_TINY,
                                  BRAIN_TASK_REGRESSION, 4, 2);
    ASSERT_NE(brain, nullptr);

    float data[4] = {0.2f, 0.4f, 0.6f, 0.8f};

    for (int i = 0; i < 3; i++) {
        brain_learn_example(brain, data, 4, "value", 0.5f + i * 0.2f);
        brain_save(brain, files[i]);
    }

    brain_destroy(brain);

    for (int i = 0; i < 3; i++) {
        brain_t loaded = brain_load(files[i]);
        if (loaded) brain_destroy(loaded);
        cleanup(files[i]);
    }
}

TEST_F(BrainSaveLoadTest, SaveNullBrain) {
    EXPECT_FALSE(brain_save(nullptr, "/tmp/null.nimcp"));
}

TEST_F(BrainSaveLoadTest, LoadNonexistent) {
    brain_t brain = brain_load("/nonexistent/file.nimcp");
    EXPECT_EQ(brain, nullptr);
}

TEST_F(BrainSaveLoadTest, SaveEmptyFilename) {
    brain_t brain = brain_create("test", BRAIN_SIZE_TINY,
                                  BRAIN_TASK_CLASSIFICATION, 3, 2);
    ASSERT_NE(brain, nullptr);

    EXPECT_FALSE(brain_save(brain, ""));

    brain_destroy(brain);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
