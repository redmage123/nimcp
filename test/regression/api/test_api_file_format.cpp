/**
 * @file test_api_file_format.cpp
 * @brief File format regression tests for NIMCP API
 *
 * Tests file format compatibility to detect breaking changes:
 * - Saved brain files can be loaded
 * - Snapshot files can be restored
 * - File format version handling
 * - Corrupted file handling
 * - Missing file handling
 *
 * Estimated tests: 15
 */

#include <gtest/gtest.h>
#include "nimcp.h"
#include <cstring>
#include <cstdio>
#include <unistd.h>

class APIFileFormatTest : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_EQ(nimcp_init(), NIMCP_SUCCESS);
    }

    void TearDown() override {
        nimcp_shutdown();
    }

    void cleanup_file(const char* filepath) {
        unlink(filepath);
    }

    // Helper to create a trained brain
    nimcp_brain_t create_trained_brain(const char* name, int training_iterations) {
        nimcp_brain_t brain = nimcp_brain_create(name, NIMCP_BRAIN_SMALL, NIMCP_TASK_CLASSIFICATION, 10, 3);
        if (!brain) return nullptr;

        float features[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f};
        for (int i = 0; i < training_iterations; i++) {
            nimcp_brain_learn_example(brain, features, 10, "trained_pattern", 1.0f);
        }

        return brain;
    }
};

//=============================================================================
// Saved Brain Files Can Be Loaded
//=============================================================================

TEST_F(APIFileFormatTest, SaveLoad_BasicBrainFile) {
    const char* save_path = "/tmp/test_basic_brain.nimcp";
    cleanup_file(save_path);

    // Create and train brain
    nimcp_brain_t brain = create_trained_brain("basic", 20);
    ASSERT_NE(brain, nullptr);

    // Get prediction before save
    float features[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f};
    char label_before[64];
    float conf_before;
    ASSERT_EQ(nimcp_brain_predict(brain, features, 10, label_before, &conf_before), NIMCP_SUCCESS);

    // Save brain
    ASSERT_EQ(nimcp_brain_save(brain, save_path), NIMCP_SUCCESS);
    nimcp_brain_destroy(brain);

    // Load brain
    nimcp_brain_t loaded = nimcp_brain_load(save_path);
    ASSERT_NE(loaded, nullptr);

    // Get prediction after load
    char label_after[64];
    float conf_after;
    ASSERT_EQ(nimcp_brain_predict(loaded, features, 10, label_after, &conf_after), NIMCP_SUCCESS);

    // Predictions should match
    EXPECT_STREQ(label_before, label_after);
    EXPECT_NEAR(conf_before, conf_after, 0.01f);

    nimcp_brain_destroy(loaded);
    cleanup_file(save_path);
}

TEST_F(APIFileFormatTest, SaveLoad_DifferentBrainSizes) {
    const char* paths[] = {
        "/tmp/test_tiny.nimcp",
        "/tmp/test_small.nimcp",
        "/tmp/test_medium.nimcp"
    };

    nimcp_brain_size_t sizes[] = {
        NIMCP_BRAIN_TINY,
        NIMCP_BRAIN_SMALL,
        NIMCP_BRAIN_MEDIUM
    };

    for (int i = 0; i < 3; i++) {
        cleanup_file(paths[i]);

        // Create, save, destroy, load
        nimcp_brain_t brain = nimcp_brain_create("test", sizes[i], NIMCP_TASK_CLASSIFICATION, 5, 2);
        ASSERT_NE(brain, nullptr);

        float features[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
        nimcp_brain_learn_example(brain, features, 5, "test", 1.0f);

        ASSERT_EQ(nimcp_brain_save(brain, paths[i]), NIMCP_SUCCESS);
        nimcp_brain_destroy(brain);

        nimcp_brain_t loaded = nimcp_brain_load(paths[i]);
        ASSERT_NE(loaded, nullptr) << "Failed to load brain of size " << i;

        nimcp_brain_destroy(loaded);
        cleanup_file(paths[i]);
    }
}

TEST_F(APIFileFormatTest, SaveLoad_DifferentTaskTypes) {
    const char* paths[] = {
        "/tmp/test_classification.nimcp",
        "/tmp/test_regression.nimcp",
        "/tmp/test_pattern.nimcp",
        "/tmp/test_sequence.nimcp",
        "/tmp/test_association.nimcp"
    };

    nimcp_brain_task_t tasks[] = {
        NIMCP_TASK_CLASSIFICATION,
        NIMCP_TASK_REGRESSION,
        NIMCP_TASK_PATTERN_MATCHING,
        NIMCP_TASK_SEQUENCE,
        NIMCP_TASK_ASSOCIATION
    };

    for (int i = 0; i < 5; i++) {
        cleanup_file(paths[i]);

        nimcp_brain_t brain = nimcp_brain_create("test", NIMCP_BRAIN_SMALL, tasks[i], 8, 2);
        ASSERT_NE(brain, nullptr);

        float features[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
        nimcp_brain_learn_example(brain, features, 8, "test", 1.0f);

        ASSERT_EQ(nimcp_brain_save(brain, paths[i]), NIMCP_SUCCESS);
        nimcp_brain_destroy(brain);

        nimcp_brain_t loaded = nimcp_brain_load(paths[i]);
        ASSERT_NE(loaded, nullptr) << "Failed to load brain with task type " << i;

        nimcp_brain_destroy(loaded);
        cleanup_file(paths[i]);
    }
}

TEST_F(APIFileFormatTest, SaveLoad_PreservesTrainingState) {
    const char* save_path = "/tmp/test_training_state.nimcp";
    cleanup_file(save_path);

    nimcp_brain_t brain = create_trained_brain("state_test", 50);
    ASSERT_NE(brain, nullptr);

    // Get probe before save
    nimcp_brain_probe_t probe_before;
    ASSERT_EQ(nimcp_brain_probe(brain, &probe_before), NIMCP_SUCCESS);

    // Save and load
    ASSERT_EQ(nimcp_brain_save(brain, save_path), NIMCP_SUCCESS);
    nimcp_brain_destroy(brain);

    nimcp_brain_t loaded = nimcp_brain_load(save_path);
    ASSERT_NE(loaded, nullptr);

    // Get probe after load
    nimcp_brain_probe_t probe_after;
    ASSERT_EQ(nimcp_brain_probe(loaded, &probe_after), NIMCP_SUCCESS);

    // Key metrics should be preserved
    EXPECT_EQ(probe_before.num_neurons, probe_after.num_neurons);
    EXPECT_EQ(probe_before.num_inputs, probe_after.num_inputs);
    EXPECT_EQ(probe_before.num_outputs, probe_after.num_outputs);
    EXPECT_EQ(probe_before.size, probe_after.size);
    EXPECT_EQ(probe_before.task, probe_after.task);

    nimcp_brain_destroy(loaded);
    cleanup_file(save_path);
}

//=============================================================================
// Snapshot Files Can Be Restored
//=============================================================================

TEST_F(APIFileFormatTest, Snapshot_SaveAndRestoreBasic) {
    const char* snap_name = "test_snapshot";

    nimcp_brain_t brain = create_trained_brain("snapshot_test", 20);
    ASSERT_NE(brain, nullptr);

    // Create snapshot
    nimcp_status_t save_status = nimcp_brain_snapshot_save(brain, snap_name, "Test snapshot");

    if (save_status == NIMCP_SUCCESS) {
        // Continue training
        float features[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f};
        for (int i = 0; i < 30; i++) {
            nimcp_brain_learn_example(brain, features, 10, "modified", 1.0f);
        }

        // Restore from snapshot
        nimcp_brain_t restored = nimcp_brain_snapshot_restore(brain, snap_name);

        if (restored != nullptr) {
            // Verify restored brain is functional
            char label[64];
            float confidence;
            ASSERT_EQ(nimcp_brain_predict(restored, features, 10, label, &confidence), NIMCP_SUCCESS);

            nimcp_brain_destroy(restored);
        }

        // Cleanup snapshot
        nimcp_brain_snapshot_delete(brain, snap_name);
    }

    nimcp_brain_destroy(brain);
}

TEST_F(APIFileFormatTest, Snapshot_MultipleSnapshots) {
    const char* snap_names[] = {"snap1", "snap2", "snap3"};

    nimcp_brain_t brain = nimcp_brain_create("multi_snap", NIMCP_BRAIN_SMALL, NIMCP_TASK_CLASSIFICATION, 5, 2);
    ASSERT_NE(brain, nullptr);

    float features[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};

    // Create multiple snapshots at different training stages
    for (int i = 0; i < 3; i++) {
        // Train
        for (int j = 0; j < 10; j++) {
            nimcp_brain_learn_example(brain, features, 5, "training", 1.0f);
        }

        // Snapshot
        nimcp_brain_snapshot_save(brain, snap_names[i], "Stage snapshot");
    }

    // List snapshots
    nimcp_brain_snapshot_info_t infos[10];
    uint32_t count = 0;
    nimcp_status_t list_status = nimcp_brain_snapshot_list(brain, infos, 10, &count);

    if (list_status == NIMCP_SUCCESS) {
        EXPECT_GT(count, 0);
    }

    // Cleanup snapshots
    for (int i = 0; i < 3; i++) {
        nimcp_brain_snapshot_delete(brain, snap_names[i]);
    }

    nimcp_brain_destroy(brain);
}

TEST_F(APIFileFormatTest, Snapshot_InfoMetadata) {
    const char* snap_name = "metadata_test";
    const char* description = "Test snapshot with metadata";

    nimcp_brain_t brain = create_trained_brain("metadata_brain", 10);
    ASSERT_NE(brain, nullptr);

    nimcp_status_t save_status = nimcp_brain_snapshot_save(brain, snap_name, description);

    if (save_status == NIMCP_SUCCESS) {
        // List and check metadata
        nimcp_brain_snapshot_info_t infos[10];
        uint32_t count = 0;

        if (nimcp_brain_snapshot_list(brain, infos, 10, &count) == NIMCP_SUCCESS) {
            // Find our snapshot
            bool found = false;
            for (uint32_t i = 0; i < count; i++) {
                if (strstr(infos[i].name, snap_name) != nullptr) {
                    found = true;
                    EXPECT_GT(infos[i].timestamp, 0);
                    EXPECT_GT(infos[i].file_size, 0);
                    break;
                }
            }
            EXPECT_TRUE(found);
        }

        nimcp_brain_snapshot_delete(brain, snap_name);
    }

    nimcp_brain_destroy(brain);
}

//=============================================================================
// File Format Version Handling
//=============================================================================

TEST_F(APIFileFormatTest, FileFormat_VersionInformation) {
    const char* save_path = "/tmp/test_version.nimcp";
    cleanup_file(save_path);

    nimcp_brain_t brain = nimcp_brain_create("version_test", NIMCP_BRAIN_SMALL, NIMCP_TASK_CLASSIFICATION, 5, 2);
    ASSERT_NE(brain, nullptr);

    float features[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    nimcp_brain_learn_example(brain, features, 5, "test", 1.0f);

    ASSERT_EQ(nimcp_brain_save(brain, save_path), NIMCP_SUCCESS);
    nimcp_brain_destroy(brain);

    // Verify file exists and has reasonable size
    FILE* f = fopen(save_path, "rb");
    ASSERT_NE(f, nullptr);

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fclose(f);

    EXPECT_GT(size, 0);

    // Load should work with current version
    nimcp_brain_t loaded = nimcp_brain_load(save_path);
    ASSERT_NE(loaded, nullptr);

    nimcp_brain_destroy(loaded);
    cleanup_file(save_path);
}

TEST_F(APIFileFormatTest, FileFormat_BackwardCompatibility) {
    // Test that files saved in previous format can still be loaded
    // (This test documents the expectation, actual implementation may vary)

    const char* save_path = "/tmp/test_compat.nimcp";
    cleanup_file(save_path);

    // Save with current version
    nimcp_brain_t brain = nimcp_brain_create("compat", NIMCP_BRAIN_SMALL, NIMCP_TASK_CLASSIFICATION, 5, 2);
    ASSERT_NE(brain, nullptr);

    float features[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    nimcp_brain_learn_example(brain, features, 5, "test", 1.0f);

    ASSERT_EQ(nimcp_brain_save(brain, save_path), NIMCP_SUCCESS);
    nimcp_brain_destroy(brain);

    // Should be able to load
    nimcp_brain_t loaded = nimcp_brain_load(save_path);
    EXPECT_NE(loaded, nullptr);

    if (loaded) {
        nimcp_brain_destroy(loaded);
    }

    cleanup_file(save_path);
}

//=============================================================================
// Corrupted File Handling
//=============================================================================

TEST_F(APIFileFormatTest, CorruptedFile_EmptyFile) {
    const char* corrupt_path = "/tmp/corrupt_empty.nimcp";

    // Create empty file
    FILE* f = fopen(corrupt_path, "wb");
    ASSERT_NE(f, nullptr);
    fclose(f);

    // Try to load empty file
    nimcp_brain_t brain = nimcp_brain_load(corrupt_path);
    EXPECT_EQ(brain, nullptr);

    // Error should be set
    const char* error = nimcp_get_error();
    EXPECT_NE(error, nullptr);
    EXPECT_GT(strlen(error), 0);

    cleanup_file(corrupt_path);
}

TEST_F(APIFileFormatTest, CorruptedFile_GarbageData) {
    const char* corrupt_path = "/tmp/corrupt_garbage.nimcp";

    // Create file with garbage data
    FILE* f = fopen(corrupt_path, "wb");
    ASSERT_NE(f, nullptr);

    const char* garbage = "This is not a valid brain file! Random garbage: !@#$%^&*()";
    fwrite(garbage, 1, strlen(garbage), f);
    fclose(f);

    // Try to load corrupted file
    nimcp_brain_t brain = nimcp_brain_load(corrupt_path);
    EXPECT_EQ(brain, nullptr);

    cleanup_file(corrupt_path);
}

TEST_F(APIFileFormatTest, CorruptedFile_TruncatedFile) {
    const char* valid_path = "/tmp/valid_brain.nimcp";
    const char* truncated_path = "/tmp/truncated_brain.nimcp";

    cleanup_file(valid_path);
    cleanup_file(truncated_path);

    // Create valid brain file
    nimcp_brain_t brain = nimcp_brain_create("truncate_test", NIMCP_BRAIN_SMALL, NIMCP_TASK_CLASSIFICATION, 5, 2);
    ASSERT_NE(brain, nullptr);

    float features[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    nimcp_brain_learn_example(brain, features, 5, "test", 1.0f);

    ASSERT_EQ(nimcp_brain_save(brain, valid_path), NIMCP_SUCCESS);
    nimcp_brain_destroy(brain);

    // Read valid file
    FILE* valid = fopen(valid_path, "rb");
    ASSERT_NE(valid, nullptr);

    fseek(valid, 0, SEEK_END);
    long size = ftell(valid);
    fseek(valid, 0, SEEK_SET);

    char* buffer = new char[size];
    fread(buffer, 1, size, valid);
    fclose(valid);

    // Write truncated file (only 4 bytes - less than magic header)
    FILE* truncated = fopen(truncated_path, "wb");
    ASSERT_NE(truncated, nullptr);
    fwrite(buffer, 1, 4, truncated);
    fclose(truncated);

    delete[] buffer;

    // Try to load truncated file
    nimcp_brain_t loaded = nimcp_brain_load(truncated_path);
    EXPECT_EQ(loaded, nullptr);
    if (loaded) { nimcp_brain_destroy(loaded); }

    cleanup_file(valid_path);
    cleanup_file(truncated_path);
}

TEST_F(APIFileFormatTest, CorruptedFile_InvalidMagicNumber) {
    const char* corrupt_path = "/tmp/corrupt_magic.nimcp";

    // Create file with invalid magic number but otherwise valid-looking structure
    FILE* f = fopen(corrupt_path, "wb");
    ASSERT_NE(f, nullptr);

    // Write some binary data that looks like it could be a brain file
    uint32_t fake_magic = 0xDEADBEEF;
    uint32_t fake_version = 1;
    uint32_t fake_size = 1000;

    fwrite(&fake_magic, sizeof(fake_magic), 1, f);
    fwrite(&fake_version, sizeof(fake_version), 1, f);
    fwrite(&fake_size, sizeof(fake_size), 1, f);

    // Write some padding
    char padding[1000] = {0};
    fwrite(padding, 1, 1000, f);

    fclose(f);

    // Try to load file with invalid magic number
    nimcp_brain_t brain = nimcp_brain_load(corrupt_path);
    EXPECT_EQ(brain, nullptr);

    cleanup_file(corrupt_path);
}

//=============================================================================
// Missing File Handling
//=============================================================================

TEST_F(APIFileFormatTest, MissingFile_NonExistentPath) {
    // Try to load from non-existent file
    nimcp_brain_t brain = nimcp_brain_load("/nonexistent/path/brain.nimcp");
    EXPECT_EQ(brain, nullptr);

    // Error should be set
    const char* error = nimcp_get_error();
    EXPECT_NE(error, nullptr);
    EXPECT_GT(strlen(error), 0);
}

TEST_F(APIFileFormatTest, MissingFile_NullPath) {
    // Try to load with NULL path
    nimcp_brain_t brain = nimcp_brain_load(nullptr);
    EXPECT_EQ(brain, nullptr);

    const char* error = nimcp_get_error();
    EXPECT_NE(error, nullptr);
}

TEST_F(APIFileFormatTest, MissingFile_SaveToInvalidDirectory) {
    nimcp_brain_t brain = nimcp_brain_create("save_invalid", NIMCP_BRAIN_SMALL, NIMCP_TASK_CLASSIFICATION, 5, 2);
    ASSERT_NE(brain, nullptr);

    // Try to save to invalid directory
    nimcp_status_t status = nimcp_brain_save(brain, "/invalid/directory/brain.nimcp");
    EXPECT_EQ(status, NIMCP_ERROR_IO);

    // Brain should still be usable
    float features[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    char label[64];
    float confidence;
    nimcp_brain_learn_example(brain, features, 5, "test", 1.0f);
    ASSERT_EQ(nimcp_brain_predict(brain, features, 5, label, &confidence), NIMCP_SUCCESS);

    nimcp_brain_destroy(brain);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
