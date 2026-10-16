/**
 * @file test_checkpoint_regression.cpp
 * @brief Regression tests for checkpoint format
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdio>
#include <unistd.h>

#include "core/brain/persistence/nimcp_checkpoint_format.h"

extern "C" {
#include "utils/memory/nimcp_memory.h"
}

/* ============================================================================
 * 1. CRC32 standard test vector (must NEVER change)
 * ============================================================================ */

TEST(CheckpointRegression, CRC32StandardVector) {
    /* IEEE 802.3 CRC32 of "123456789" = 0xCBF43926 */
    EXPECT_EQ(nimcp_crc32("123456789", 9), 0xCBF43926u);
}

/* ============================================================================
 * 2. Header layout backward compatibility
 * ============================================================================ */

TEST(CheckpointRegression, HeaderLayout) {
    /* These must NEVER change — they define the on-disk format */
    EXPECT_EQ(sizeof(nimcp_checkpoint_header_t), 64u);
    EXPECT_EQ(sizeof(nimcp_section_entry_t), 48u);
    EXPECT_EQ(NIMCP_UNIFIED_MAGIC, 0x4E494D56u);
    EXPECT_EQ(NIMCP_UNIFIED_VERSION, 1u);
    EXPECT_EQ(NIMCP_SECTION_NAME_LEN, 32u);
}

/* ============================================================================
 * 3. Legacy magic still recognized
 * ============================================================================ */

TEST(CheckpointRegression, LegacyMagicRecognized) {
    EXPECT_EQ(NIMCP_LEGACY_MAGIC, 0x4E494D43u);
    EXPECT_NE(NIMCP_UNIFIED_MAGIC, NIMCP_LEGACY_MAGIC);
}

/* ============================================================================
 * 4. CRC32 of zero bytes is zero
 * ============================================================================ */

TEST(CheckpointRegression, CRC32ZeroBytes) {
    EXPECT_EQ(nimcp_crc32(NULL, 0), 0x00000000u);
}

/* ============================================================================
 * 5. Large CRC32 doesn't crash
 * ============================================================================ */

TEST(CheckpointRegression, CRC32LargeBuffer) {
    /* 1MB buffer */
    void* buf = calloc(1, 1024 * 1024);
    ASSERT_NE(buf, nullptr);
    uint32_t crc = nimcp_crc32(buf, 1024 * 1024);
    EXPECT_NE(crc, 0u); /* Zero buffer has nonzero CRC */
    free(buf);
}
