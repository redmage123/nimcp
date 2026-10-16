/**
 * @file test_persistence_short_read.cpp
 * @brief Tests for persistence short-read handling (Bug H1)
 *
 * WHAT: Verify that persistence functions handle truncated/corrupt files gracefully
 * WHY:  Previously, unchecked fread() returns could silently produce corrupt brain state
 * HOW:  Create truncated files and verify brain_load / nimcp_brain_load_metadata return
 *       failure (not success with garbage data)
 */

#include <gtest/gtest.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <filesystem>

// Headers have their own extern "C" guards
#include "core/brain/nimcp_brain.h"
#include "core/brain/persistence/nimcp_brain_persistence.h"

class PersistenceShortReadTest : public ::testing::Test {
protected:
    std::string test_dir;
    std::string test_base_path;

    void SetUp() override {
        // Create a unique temp directory for this test
        test_dir = std::string("/tmp/nimcp_persist_test_") + std::to_string(getpid());
        std::filesystem::create_directories(test_dir);
        test_base_path = test_dir + "/test_brain";
    }

    void TearDown() override {
        // Clean up test files
        std::filesystem::remove_all(test_dir);
    }

    /**
     * WHAT: Write a file with specific number of bytes
     * WHY:  Simulate truncated persistence files
     */
    void write_truncated_file(const std::string& path, size_t num_bytes) {
        FILE* f = fopen(path.c_str(), "wb");
        ASSERT_NE(f, nullptr) << "Failed to create test file: " << path;

        // Write random-ish data (zero-initialized is fine for testing failure paths)
        std::vector<uint8_t> data(num_bytes, 0xAB);
        if (num_bytes > 0) {
            fwrite(data.data(), 1, num_bytes, f);
        }
        fclose(f);
    }
};

/**
 * WHAT: Test that loading from a nonexistent file returns NULL
 * WHY:  Basic sanity check — file-not-found must not crash
 */
TEST_F(PersistenceShortReadTest, LoadNonexistentFileReturnsNull) {
    brain_t brain = brain_load("/tmp/nimcp_nonexistent_brain_12345.bin");
    EXPECT_EQ(brain, nullptr)
        << "brain_load on nonexistent file must return NULL";
}

/**
 * WHAT: Test that loading from an empty file returns NULL
 * WHY:  Empty file means zero bytes readable — all fread() calls should fail
 */
TEST_F(PersistenceShortReadTest, LoadEmptyFileReturnsNull) {
    std::string filepath = test_base_path + ".bin";
    write_truncated_file(filepath, 0);

    brain_t brain = brain_load(test_base_path.c_str());
    EXPECT_EQ(brain, nullptr)
        << "brain_load on empty file must return NULL";
}

/**
 * WHAT: Test that loading from a very small file (truncated header) returns NULL
 * WHY:  If the file is smaller than the header structure, fread must fail
 */
TEST_F(PersistenceShortReadTest, LoadTruncatedHeaderReturnsNull) {
    // Write just a few bytes — not enough for any valid header
    std::string filepath = test_base_path + ".bin";
    write_truncated_file(filepath, 8);

    brain_t brain = brain_load(test_base_path.c_str());
    EXPECT_EQ(brain, nullptr)
        << "brain_load on truncated header must return NULL";
}

/**
 * WHAT: Test that loading from a file with partial data returns NULL
 * WHY:  A file that has a plausible size but garbage content should still fail safely
 */
TEST_F(PersistenceShortReadTest, LoadGarbageDataReturnsNull) {
    // Write enough bytes that it might look like a header but is actually garbage
    std::string filepath = test_base_path + ".bin";
    write_truncated_file(filepath, 512);

    brain_t brain = brain_load(test_base_path.c_str());
    EXPECT_EQ(brain, nullptr)
        << "brain_load on garbage data must return NULL";
}

/**
 * WHAT: Test that loading truncated metadata returns false
 * WHY:  The Bug H1 fix added fread() checks to nimcp_brain_load_metadata;
 *       this verifies those checks work
 */
TEST_F(PersistenceShortReadTest, LoadTruncatedMetadataReturnsNull) {
    // Create a .meta file with truncated data
    std::string meta_filepath = test_base_path + ".meta";
    write_truncated_file(meta_filepath, 32);

    // Also create main .bin file so brain_load doesn't fail on missing primary file
    std::string bin_filepath = test_base_path + ".bin";
    write_truncated_file(bin_filepath, 256);

    brain_t brain = brain_load(test_base_path.c_str());
    EXPECT_EQ(brain, nullptr)
        << "brain_load with truncated metadata file must return NULL";
}

/**
 * WHAT: Test NULL filepath handling
 * WHY:  Verify guard clauses prevent NULL pointer dereference
 */
TEST_F(PersistenceShortReadTest, LoadNullFilepathReturnsNull) {
    brain_t brain = brain_load(nullptr);
    EXPECT_EQ(brain, nullptr)
        << "brain_load(NULL) must return NULL";
}
