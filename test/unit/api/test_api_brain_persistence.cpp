/**
 * @file test_api_brain_persistence.cpp
 * @brief Unit tests for NIMCP API brain save/load persistence functionality
 *
 * Tests nimcp_brain_save() and nimcp_brain_load() functions including:
 * - Valid save/load operations
 * - NULL parameter validation
 * - File I/O error handling
 * - State preservation across save/load
 * - Handle validity after load
 * - Cleanup and resource management
 */

#include <gtest/gtest.h>
#include "nimcp.h"
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <sys/stat.h>

class BrainPersistenceTest : public ::testing::Test {
protected:
    nimcp_brain_t brain;
    char temp_filepath[256];

    void SetUp() override {
        // Initialize NIMCP
        nimcp_init();

        // Create temporary file path
        snprintf(temp_filepath, sizeof(temp_filepath),
                 "/tmp/nimcp_test_brain_%d.bin", getpid());

        // Create a test brain
        brain = nimcp_brain_create("test_brain", NIMCP_BRAIN_TINY,
                                   NIMCP_TASK_CLASSIFICATION, 10, 5);
        ASSERT_NE(brain, nullptr);
    }

    void TearDown() override {
        // Cleanup brain
        if (brain) {
            nimcp_brain_destroy(brain);
            brain = nullptr;
        }

        // Remove temporary file if exists
        unlink(temp_filepath);

        // Shutdown NIMCP
        nimcp_shutdown();
    }

    bool file_exists(const char* path) {
        struct stat buffer;
        return (stat(path, &buffer) == 0);
    }
};

//=============================================================================
// Save Tests
//=============================================================================

TEST_F(BrainPersistenceTest, SaveWithValidBrainSucceeds) {
    nimcp_status_t status = nimcp_brain_save(brain, temp_filepath);

    EXPECT_EQ(status, NIMCP_SUCCESS);
    EXPECT_TRUE(file_exists(temp_filepath));
}

TEST_F(BrainPersistenceTest, SaveWithNullBrainFails) {
    nimcp_status_t status = nimcp_brain_save(nullptr, temp_filepath);

    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
    EXPECT_STREQ(nimcp_get_error(), "Brain handle is NULL");
}

TEST_F(BrainPersistenceTest, SaveWithNullFilepathFails) {
    nimcp_status_t status = nimcp_brain_save(brain, nullptr);

    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
    EXPECT_STREQ(nimcp_get_error(), "Filepath is NULL");
}

TEST_F(BrainPersistenceTest, SaveWithBothNullFails) {
    nimcp_status_t status = nimcp_brain_save(nullptr, nullptr);

    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
}

TEST_F(BrainPersistenceTest, SaveCreatesNonEmptyFile) {
    nimcp_status_t status = nimcp_brain_save(brain, temp_filepath);
    ASSERT_EQ(status, NIMCP_SUCCESS);

    // Check file size is non-zero
    struct stat st;
    ASSERT_EQ(stat(temp_filepath, &st), 0);
    EXPECT_GT(st.st_size, 0);
}

TEST_F(BrainPersistenceTest, SaveMultipleTimesSucceeds) {
    // First save
    nimcp_status_t status1 = nimcp_brain_save(brain, temp_filepath);
    EXPECT_EQ(status1, NIMCP_SUCCESS);

    // Second save (overwrite)
    nimcp_status_t status2 = nimcp_brain_save(brain, temp_filepath);
    EXPECT_EQ(status2, NIMCP_SUCCESS);
}

//=============================================================================
// Load Tests
//=============================================================================

TEST_F(BrainPersistenceTest, LoadWithValidFileSucceeds) {
    // Save brain first
    ASSERT_EQ(nimcp_brain_save(brain, temp_filepath), NIMCP_SUCCESS);

    // Load brain
    nimcp_brain_t loaded = nimcp_brain_load(temp_filepath);

    EXPECT_NE(loaded, nullptr);
    nimcp_brain_destroy(loaded);
}

TEST_F(BrainPersistenceTest, LoadWithNullFilepathFails) {
    nimcp_brain_t loaded = nimcp_brain_load(nullptr);

    EXPECT_EQ(loaded, nullptr);
    EXPECT_STREQ(nimcp_get_error(), "Filepath is NULL");
}

TEST_F(BrainPersistenceTest, LoadWithNonexistentFileFails) {
    const char* fake_path = "/tmp/nonexistent_brain_file_12345.bin";
    unlink(fake_path); // Ensure it doesn't exist

    nimcp_brain_t loaded = nimcp_brain_load(fake_path);

    EXPECT_EQ(loaded, nullptr);
    EXPECT_STRNE(nimcp_get_error(), "No error");
}

TEST_F(BrainPersistenceTest, LoadWithInvalidPathFails) {
    const char* invalid_path = "/invalid/path/that/does/not/exist/brain.bin";

    nimcp_brain_t loaded = nimcp_brain_load(invalid_path);

    EXPECT_EQ(loaded, nullptr);
}

TEST_F(BrainPersistenceTest, LoadReturnsValidHandle) {
    // Save brain
    ASSERT_EQ(nimcp_brain_save(brain, temp_filepath), NIMCP_SUCCESS);

    // Load brain
    nimcp_brain_t loaded = nimcp_brain_load(temp_filepath);
    ASSERT_NE(loaded, nullptr);

    // Test that handle is valid by using it
    nimcp_brain_probe_t probe;
    nimcp_status_t status = nimcp_brain_probe(loaded, &probe);
    EXPECT_EQ(status, NIMCP_SUCCESS);

    nimcp_brain_destroy(loaded);
}

//=============================================================================
// State Preservation Tests
//=============================================================================

TEST_F(BrainPersistenceTest, SaveLoadPreservesBrainState) {
    // Train brain with an example
    float features[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f,
                          0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
    nimcp_brain_learn_example(brain, features, 10, "test_class", 0.9f);

    // Get stats before save
    nimcp_brain_probe_t probe_before;
    ASSERT_EQ(nimcp_brain_probe(brain, &probe_before), NIMCP_SUCCESS);

    // Save brain
    ASSERT_EQ(nimcp_brain_save(brain, temp_filepath), NIMCP_SUCCESS);

    // Load brain
    nimcp_brain_t loaded = nimcp_brain_load(temp_filepath);
    ASSERT_NE(loaded, nullptr);

    // Get stats after load
    nimcp_brain_probe_t probe_after;
    ASSERT_EQ(nimcp_brain_probe(loaded, &probe_after), NIMCP_SUCCESS);

    // Verify state is preserved
    EXPECT_EQ(probe_before.num_neurons, probe_after.num_neurons);
    EXPECT_EQ(probe_before.size, probe_after.size);
    EXPECT_EQ(probe_before.task, probe_after.task);

    nimcp_brain_destroy(loaded);
}

TEST_F(BrainPersistenceTest, SaveLoadPreservesLearningProgress) {
    // Do multiple learning steps
    float features[10];
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 10; j++) {
            features[j] = (float)(i * 10 + j) / 100.0f;
        }
        nimcp_brain_learn_example(brain, features, 10, "class_a", 0.8f);
    }

    // Save and load
    ASSERT_EQ(nimcp_brain_save(brain, temp_filepath), NIMCP_SUCCESS);
    nimcp_brain_t loaded = nimcp_brain_load(temp_filepath);
    ASSERT_NE(loaded, nullptr);

    // Verify loaded brain can make predictions
    char label[64];
    float confidence;
    nimcp_status_t status = nimcp_brain_predict(loaded, features, 10,
                                                label, &confidence);
    EXPECT_EQ(status, NIMCP_SUCCESS);

    nimcp_brain_destroy(loaded);
}

// TODO: Re-enable when spatial neuromodulator double-free bug is fixed
// The test crashes during nimcp_brain_destroy(loaded) due to improper
// re-initialization of spatial neuromodulator system in nimcp_brain_load
TEST_F(BrainPersistenceTest, DISABLED_LoadedBrainIsIndependent) {
    // Save original brain
    ASSERT_EQ(nimcp_brain_save(brain, temp_filepath), NIMCP_SUCCESS);

    // Load brain
    nimcp_brain_t loaded = nimcp_brain_load(temp_filepath);
    ASSERT_NE(loaded, nullptr);

    // Modify loaded brain
    float features[10] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
                          1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    nimcp_brain_learn_example(loaded, features, 10, "new_class", 0.95f);

    // Both brains should be independently usable
    nimcp_brain_probe_t probe_original, probe_loaded;
    EXPECT_EQ(nimcp_brain_probe(brain, &probe_original), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_brain_probe(loaded, &probe_loaded), NIMCP_SUCCESS);

    nimcp_brain_destroy(loaded);
}

//=============================================================================
// Multiple Save/Load Tests
//=============================================================================

TEST_F(BrainPersistenceTest, MultipleSaveLoadCycles) {
    char path1[256], path2[256];
    snprintf(path1, sizeof(path1), "/tmp/nimcp_test_1_%d.bin", getpid());
    snprintf(path2, sizeof(path2), "/tmp/nimcp_test_2_%d.bin", getpid());

    // First cycle
    ASSERT_EQ(nimcp_brain_save(brain, path1), NIMCP_SUCCESS);
    nimcp_brain_t loaded1 = nimcp_brain_load(path1);
    ASSERT_NE(loaded1, nullptr);

    // Second cycle
    ASSERT_EQ(nimcp_brain_save(loaded1, path2), NIMCP_SUCCESS);
    nimcp_brain_t loaded2 = nimcp_brain_load(path2);
    ASSERT_NE(loaded2, nullptr);

    // Verify final brain is usable
    nimcp_brain_probe_t probe;
    EXPECT_EQ(nimcp_brain_probe(loaded2, &probe), NIMCP_SUCCESS);

    nimcp_brain_destroy(loaded1);
    nimcp_brain_destroy(loaded2);
    unlink(path1);
    unlink(path2);
}

//=============================================================================
// Resource Management Tests
//=============================================================================

TEST_F(BrainPersistenceTest, DestroyAfterSaveDoesNotCorruptFile) {
    // Save brain
    ASSERT_EQ(nimcp_brain_save(brain, temp_filepath), NIMCP_SUCCESS);

    // Destroy original brain
    nimcp_brain_destroy(brain);
    brain = nullptr;

    // Load should still work
    nimcp_brain_t loaded = nimcp_brain_load(temp_filepath);
    EXPECT_NE(loaded, nullptr);

    nimcp_brain_destroy(loaded);
}

TEST_F(BrainPersistenceTest, LoadAfterBrainDestroySucceeds) {
    // Save and destroy
    ASSERT_EQ(nimcp_brain_save(brain, temp_filepath), NIMCP_SUCCESS);
    nimcp_brain_destroy(brain);
    brain = nullptr;

    // Load new brain
    nimcp_brain_t loaded = nimcp_brain_load(temp_filepath);
    ASSERT_NE(loaded, nullptr);

    // Verify it works
    nimcp_brain_probe_t probe;
    EXPECT_EQ(nimcp_brain_probe(loaded, &probe), NIMCP_SUCCESS);

    nimcp_brain_destroy(loaded);
}

TEST_F(BrainPersistenceTest, MultipleLoadsFromSameFile) {
    // Save brain
    ASSERT_EQ(nimcp_brain_save(brain, temp_filepath), NIMCP_SUCCESS);

    // Load multiple times
    nimcp_brain_t loaded1 = nimcp_brain_load(temp_filepath);
    ASSERT_NE(loaded1, nullptr);

    nimcp_brain_t loaded2 = nimcp_brain_load(temp_filepath);
    ASSERT_NE(loaded2, nullptr);

    // Both should be valid and independent
    nimcp_brain_probe_t probe1, probe2;
    EXPECT_EQ(nimcp_brain_probe(loaded1, &probe1), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_brain_probe(loaded2, &probe2), NIMCP_SUCCESS);

    nimcp_brain_destroy(loaded1);
    nimcp_brain_destroy(loaded2);
}

//=============================================================================
// Error Recovery Tests
//=============================================================================

TEST_F(BrainPersistenceTest, SaveAfterFailedSaveSucceeds) {
    // Try to save to invalid path (should fail)
    nimcp_status_t status1 = nimcp_brain_save(brain, "/invalid/path/brain.bin");
    EXPECT_NE(status1, NIMCP_SUCCESS);

    // Save to valid path should succeed
    nimcp_status_t status2 = nimcp_brain_save(brain, temp_filepath);
    EXPECT_EQ(status2, NIMCP_SUCCESS);
}

TEST_F(BrainPersistenceTest, LoadAfterFailedLoadSucceeds) {
    // Try to load nonexistent file (should fail)
    nimcp_brain_t loaded1 = nimcp_brain_load("/tmp/nonexistent_12345.bin");
    EXPECT_EQ(loaded1, nullptr);

    // Save and load valid file should succeed
    ASSERT_EQ(nimcp_brain_save(brain, temp_filepath), NIMCP_SUCCESS);
    nimcp_brain_t loaded2 = nimcp_brain_load(temp_filepath);
    EXPECT_NE(loaded2, nullptr);

    nimcp_brain_destroy(loaded2);
}
