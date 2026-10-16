/**
 * @file test_unified_checkpoint_integration.cpp
 * @brief Integration tests for unified checkpoint save/load with real brain
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdio>
#include <sys/stat.h>
#include <unistd.h>

#include "core/brain/persistence/nimcp_checkpoint_format.h"

extern "C" {
#include "utils/memory/nimcp_memory.h"
#include "nimcp.h"
}

#include "core/brain/nimcp_brain_internal.h"

/* Expose opaque handle */
struct nimcp_brain_handle {
    brain_t internal_brain;
    float   last_loss;
    float   last_gradient_norm;
};

class UnifiedCheckpointIntegration : public ::testing::Test {
protected:
    nimcp_brain_t brain_handle;
    char test_path[256];

    void SetUp() override {
        brain_handle = NULL;
        snprintf(test_path, sizeof(test_path), "/tmp/nimcp_ckpt_test_%d.bin", getpid());
        nimcp_init();
    }

    void TearDown() override {
        if (brain_handle) {
            nimcp_brain_destroy(brain_handle);
            brain_handle = NULL;
        }
        unlink(test_path);
        /* Clean up temp files */
        char tmp[512];
        snprintf(tmp, sizeof(tmp), "%s.unified_tmp", test_path);
        unlink(tmp);
        nimcp_shutdown();
    }

    bool create_brain() {
        brain_handle = nimcp_brain_create("test_ckpt",
                                           NIMCP_BRAIN_SMALL,
                                           NIMCP_TASK_CLASSIFICATION,
                                           64, 64);
        return brain_handle != NULL;
    }
};

/* ============================================================================
 * 1. Save creates a file
 * ============================================================================ */

TEST_F(UnifiedCheckpointIntegration, SaveCreatesFile) {
    if (!create_brain()) GTEST_SKIP() << "Brain creation failed";

    nimcp_status_t status = nimcp_brain_save(brain_handle, test_path);
    EXPECT_EQ(status, NIMCP_OK);

    struct stat st;
    EXPECT_EQ(stat(test_path, &st), 0);
    EXPECT_GT(st.st_size, 0);
}

/* ============================================================================
 * 2. Saved file has unified magic
 * ============================================================================ */

TEST_F(UnifiedCheckpointIntegration, SavedFileHasUnifiedMagic) {
    if (!create_brain()) GTEST_SKIP() << "Brain creation failed";

    nimcp_brain_save(brain_handle, test_path);

    FILE* f = fopen(test_path, "rb");
    ASSERT_NE(f, nullptr);
    uint32_t magic = 0;
    fread(&magic, sizeof(magic), 1, f);
    fclose(f);

    EXPECT_EQ(magic, NIMCP_UNIFIED_MAGIC);
}

/* ============================================================================
 * 3. Save + load round-trip
 * ============================================================================ */

TEST_F(UnifiedCheckpointIntegration, SaveLoadRoundTrip) {
    if (!create_brain()) GTEST_SKIP() << "Brain creation failed";

    nimcp_brain_save(brain_handle, test_path);

    /* Destroy original */
    nimcp_brain_destroy(brain_handle);
    brain_handle = NULL;

    /* Load from unified checkpoint */
    brain_handle = nimcp_brain_load(test_path);
    ASSERT_NE(brain_handle, nullptr);
    ASSERT_NE(brain_handle->internal_brain, nullptr);
}

/* ============================================================================
 * 4. Loaded brain has correct neuron count
 * ============================================================================ */

TEST_F(UnifiedCheckpointIntegration, LoadedBrainNeuronCount) {
    if (!create_brain()) GTEST_SKIP() << "Brain creation failed";

    uint32_t original_count = nimcp_brain_get_neuron_count(brain_handle);
    nimcp_brain_save(brain_handle, test_path);
    nimcp_brain_destroy(brain_handle);

    brain_handle = nimcp_brain_load(test_path);
    ASSERT_NE(brain_handle, nullptr);

    uint32_t loaded_count = nimcp_brain_get_neuron_count(brain_handle);
    EXPECT_EQ(loaded_count, original_count);
}

/* ============================================================================
 * 5. No temp files left after save
 * ============================================================================ */

TEST_F(UnifiedCheckpointIntegration, NoTempFilesAfterSave) {
    if (!create_brain()) GTEST_SKIP() << "Brain creation failed";

    nimcp_brain_save(brain_handle, test_path);

    /* Check no ._sec_ temp files remain */
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s._sec_brain_core", test_path);
    EXPECT_NE(access(tmp, F_OK), 0) << "Temp file not cleaned up: " << tmp;

    snprintf(tmp, sizeof(tmp), "%s.unified_tmp", test_path);
    EXPECT_NE(access(tmp, F_OK), 0) << "Unified tmp not renamed: " << tmp;
}
