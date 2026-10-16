/**
 * @file test_unified_checkpoint.cpp
 * @brief Unit tests for unified checkpoint format (header, CRC32, sections)
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
 * 1. Header struct size must be exactly 64 bytes
 * ============================================================================ */

TEST(UnifiedCheckpoint, HeaderSizeIs64) {
    EXPECT_EQ(sizeof(nimcp_checkpoint_header_t), 64u);
}

/* ============================================================================
 * 2. Section entry struct size must be exactly 48 bytes
 * ============================================================================ */

TEST(UnifiedCheckpoint, SectionEntryIs48) {
    EXPECT_EQ(sizeof(nimcp_section_entry_t), 48u);
}

/* ============================================================================
 * 3. Magic constant matches expected value
 * ============================================================================ */

TEST(UnifiedCheckpoint, MagicConstants) {
    EXPECT_EQ(NIMCP_UNIFIED_MAGIC, 0x4E494D56u);
    EXPECT_EQ(NIMCP_LEGACY_MAGIC, 0x4E494D43u);
    /* Unified and legacy magic differ only in last byte */
    EXPECT_NE(NIMCP_UNIFIED_MAGIC, NIMCP_LEGACY_MAGIC);
}

/* ============================================================================
 * 4. CRC32 known test vectors
 * ============================================================================ */

TEST(UnifiedCheckpoint, CRC32KnownVectors) {
    /* CRC32 of empty string should be 0 */
    uint32_t crc_empty = nimcp_crc32("", 0);
    EXPECT_EQ(crc_empty, 0x00000000u);

    /* CRC32 of "123456789" = 0xCBF43926 (standard test vector) */
    uint32_t crc_test = nimcp_crc32("123456789", 9);
    EXPECT_EQ(crc_test, 0xCBF43926u);
}

/* ============================================================================
 * 5. CRC32 detects single-byte corruption
 * ============================================================================ */

TEST(UnifiedCheckpoint, CRC32DetectsCorruption) {
    const char data[] = "Hello, NIMCP checkpoint!";
    uint32_t crc_original = nimcp_crc32(data, strlen(data));

    char corrupted[64];
    memcpy(corrupted, data, strlen(data));
    corrupted[5] = 'X';  /* corrupt one byte */
    uint32_t crc_corrupted = nimcp_crc32(corrupted, strlen(data));

    EXPECT_NE(crc_original, crc_corrupted);
}

/* ============================================================================
 * 6. CRC32 file matches buffer CRC
 * ============================================================================ */

TEST(UnifiedCheckpoint, CRC32FileMatchesBuffer) {
    const char data[] = "Test data for CRC32 file computation";
    uint32_t crc_buf = nimcp_crc32(data, strlen(data));

    /* Write to temp file */
    char tmp[] = "/tmp/nimcp_test_crc32_XXXXXX";
    int fd = mkstemp(tmp);
    ASSERT_GE(fd, 0);
    write(fd, data, strlen(data));
    close(fd);

    /* Compute CRC from file */
    FILE* fp = fopen(tmp, "rb");
    ASSERT_NE(fp, nullptr);
    uint32_t crc_file = nimcp_crc32_file(fp, 0, strlen(data));
    fclose(fp);
    unlink(tmp);

    EXPECT_EQ(crc_buf, crc_file);
}

/* ============================================================================
 * 7. Header zero-initialization
 * ============================================================================ */

TEST(UnifiedCheckpoint, HeaderZeroInit) {
    nimcp_checkpoint_header_t header;
    memset(&header, 0, sizeof(header));
    EXPECT_EQ(header.magic, 0u);
    EXPECT_EQ(header.num_sections, 0u);
    EXPECT_EQ(header.checksum, 0u);
}

/* ============================================================================
 * 8. Section entry name truncation
 * ============================================================================ */

TEST(UnifiedCheckpoint, SectionNameTruncation) {
    nimcp_section_entry_t entry;
    memset(&entry, 0, sizeof(entry));

    /* Short name fits */
    strncpy(entry.name, "snn", NIMCP_SECTION_NAME_LEN - 1);
    EXPECT_STREQ(entry.name, "snn");

    /* Long name truncated */
    strncpy(entry.name, "this_is_a_very_long_section_name_that_exceeds_32_chars",
            NIMCP_SECTION_NAME_LEN - 1);
    entry.name[NIMCP_SECTION_NAME_LEN - 1] = '\0';
    EXPECT_EQ(strlen(entry.name), NIMCP_SECTION_NAME_LEN - 1u);
}

/* ============================================================================
 * 9. Max sections constant
 * ============================================================================ */

TEST(UnifiedCheckpoint, MaxSections) {
    EXPECT_GE(NIMCP_MAX_SECTIONS, 12u); /* At least 12 for current sidecars */
    EXPECT_LE(NIMCP_MAX_SECTIONS, 64u); /* Reasonable upper bound */
}

/* ============================================================================
 * 10. Section name constants are valid strings
 * ============================================================================ */

TEST(UnifiedCheckpoint, SectionNameConstants) {
    EXPECT_STREQ(NIMCP_SEC_BRAIN_CORE, "brain_core");
    EXPECT_STREQ(NIMCP_SEC_META, "meta");
    EXPECT_STREQ(NIMCP_SEC_SNN, "snn");
    EXPECT_STREQ(NIMCP_SEC_LNN, "lnn");
    EXPECT_STREQ(NIMCP_SEC_CNN, "cnn");
    EXPECT_STREQ(NIMCP_SEC_MIRROR, "mirror_neurons");

    /* All names fit in section entry */
    EXPECT_LT(strlen(NIMCP_SEC_BRAIN_CORE), (size_t)NIMCP_SECTION_NAME_LEN);
    EXPECT_LT(strlen(NIMCP_SEC_MIRROR), (size_t)NIMCP_SECTION_NAME_LEN);
}
