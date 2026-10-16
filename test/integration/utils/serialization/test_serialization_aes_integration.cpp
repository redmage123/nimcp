/**
 * @file test_serialization_aes_integration.cpp
 * @brief Integration tests for AES-256-GCM encryption in nimcp_serialization
 *
 * WHAT: Tests that AES encrypt/decrypt integrates correctly with
 *       serialization contexts, file I/O, and multi-buffer workflows
 * WHY:  Unit tests verify individual functions; these verify components
 *       working together end-to-end through the serialization layer
 * HOW:  Use separate contexts, file persistence, nimcp_write_processed/
 *       nimcp_read_processed with encryption and compression flags
 *
 * @date 2026-02-12
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <vector>
#include <string>

// Header has its own extern "C" guard
#include "utils/serialization/nimcp_serialization.h"

// Format flag constants (from nimcp_brain.h, reproduced here to avoid
// pulling in heavy brain dependencies)
#define NIMCP_FORMAT_FLAG_COMPRESSED  0x00000001
#define NIMCP_FORMAT_FLAG_ENCRYPTED   0x00000002

namespace {

//=============================================================================
// Test Fixture
//=============================================================================

class SerializationAesIntegrationTest : public ::testing::Test {
protected:
    uint8_t key1_[32];
    uint8_t key2_[32];

    void SetUp() override {
        for (size_t i = 0; i < 32; i++) {
            key1_[i] = static_cast<uint8_t>(0x42 + i);
        }
        for (size_t i = 0; i < 32; i++) {
            key2_[i] = static_cast<uint8_t>(0xBB + i);
        }
    }

    // Helper: create a temp file path
    std::string make_temp_path(const char* suffix) {
        char path[256];
        snprintf(path, sizeof(path), "/tmp/nimcp_aes_integration_%d_%s",
                 static_cast<int>(getpid()), suffix);
        return std::string(path);
    }

    // Helper: generate deterministic test data
    std::vector<uint8_t> make_test_data(size_t size, uint8_t seed = 0x37) {
        std::vector<uint8_t> data(size);
        for (size_t i = 0; i < size; i++) {
            data[i] = static_cast<uint8_t>((i * 17 + seed) % 256);
        }
        return data;
    }
};

//=============================================================================
// Cross-Context Tests
//=============================================================================

TEST_F(SerializationAesIntegrationTest, EncryptWithOneContextDecryptWithAnother) {
    // WHAT: Encrypt data using one context, decrypt with a separate context
    //       that has the same key
    // WHY:  Verifies context state is independent - encryption output carries
    //       all necessary info (IV, tag) for any context with the right key

    if (!nimcp_aes_available()) {
        GTEST_SKIP() << "AES not available";
    }

    // Set up encryption context
    nimcp_serialize_ctx_t enc_ctx;
    nimcp_serialize_ctx_init(&enc_ctx);
    nimcp_serialize_ctx_set_key(&enc_ctx, key1_, sizeof(key1_));
    ASSERT_TRUE(enc_ctx.key_set);

    // Set up decryption context (separate object, same key)
    nimcp_serialize_ctx_t dec_ctx;
    nimcp_serialize_ctx_init(&dec_ctx);
    nimcp_serialize_ctx_set_key(&dec_ctx, key1_, sizeof(key1_));
    ASSERT_TRUE(dec_ctx.key_set);

    const char* message = "Cross-context integration test payload";
    size_t plaintext_size = strlen(message) + 1;

    // Encrypt using enc_ctx key
    size_t encrypted_size = 0;
    uint8_t* encrypted = nimcp_encrypt(
        reinterpret_cast<const uint8_t*>(message), plaintext_size,
        enc_ctx.key, enc_ctx.key_size, &encrypted_size);
    ASSERT_NE(encrypted, nullptr);
    EXPECT_GT(encrypted_size, plaintext_size);

    // Decrypt using dec_ctx key (separate context object)
    size_t decrypted_size = 0;
    uint8_t* decrypted = nimcp_decrypt(
        encrypted, encrypted_size,
        dec_ctx.key, dec_ctx.key_size, &decrypted_size);
    ASSERT_NE(decrypted, nullptr);
    EXPECT_EQ(decrypted_size, plaintext_size);
    EXPECT_EQ(memcmp(decrypted, message, plaintext_size), 0)
        << "Data decrypted with separate context must match original";

    free(encrypted);
    free(decrypted);
}

TEST_F(SerializationAesIntegrationTest, ContextWithDifferentKeyFails) {
    // WHAT: Encrypt with one context, try decrypt with a context that has
    //       a different key
    // WHY:  Verifies GCM authentication catches key mismatch across contexts

    if (!nimcp_aes_available()) {
        GTEST_SKIP() << "AES not available";
    }

    nimcp_serialize_ctx_t enc_ctx;
    nimcp_serialize_ctx_init(&enc_ctx);
    nimcp_serialize_ctx_set_key(&enc_ctx, key1_, sizeof(key1_));

    nimcp_serialize_ctx_t dec_ctx;
    nimcp_serialize_ctx_init(&dec_ctx);
    nimcp_serialize_ctx_set_key(&dec_ctx, key2_, sizeof(key2_));

    uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE};

    size_t encrypted_size = 0;
    uint8_t* encrypted = nimcp_encrypt(
        data, sizeof(data),
        enc_ctx.key, enc_ctx.key_size, &encrypted_size);
    ASSERT_NE(encrypted, nullptr);

    // Decrypt with wrong-key context must fail
    size_t decrypted_size = 0;
    uint8_t* decrypted = nimcp_decrypt(
        encrypted, encrypted_size,
        dec_ctx.key, dec_ctx.key_size, &decrypted_size);
    EXPECT_EQ(decrypted, nullptr)
        << "Decryption with different-key context must fail";

    free(encrypted);
    if (decrypted) free(decrypted);
}

//=============================================================================
// File Persistence Round-Trip Tests
//=============================================================================

TEST_F(SerializationAesIntegrationTest, EncryptWriteReadDecryptRoundTrip) {
    // WHAT: Encrypt data, write ciphertext to file, read back, decrypt
    // WHY:  Validates the full persistence workflow that brain save/load uses

    if (!nimcp_aes_available()) {
        GTEST_SKIP() << "AES not available";
    }

    auto data = make_test_data(512);
    std::string path = make_temp_path("roundtrip.bin");

    // Encrypt
    size_t encrypted_size = 0;
    uint8_t* encrypted = nimcp_encrypt(
        data.data(), data.size(),
        key1_, sizeof(key1_), &encrypted_size);
    ASSERT_NE(encrypted, nullptr);
    ASSERT_GT(encrypted_size, 0u);

    // Write encrypted data to file
    FILE* wf = fopen(path.c_str(), "wb");
    ASSERT_NE(wf, nullptr) << "Failed to open temp file for writing";
    size_t written = fwrite(encrypted, 1, encrypted_size, wf);
    fclose(wf);
    ASSERT_EQ(written, encrypted_size);

    // Read encrypted data back from file
    FILE* rf = fopen(path.c_str(), "rb");
    ASSERT_NE(rf, nullptr) << "Failed to open temp file for reading";
    std::vector<uint8_t> read_back(encrypted_size);
    size_t read_count = fread(read_back.data(), 1, encrypted_size, rf);
    fclose(rf);
    ASSERT_EQ(read_count, encrypted_size);

    // Verify file content matches encrypted data
    EXPECT_EQ(memcmp(read_back.data(), encrypted, encrypted_size), 0)
        << "File content must match encrypted data exactly";

    // Decrypt the data read from file
    size_t decrypted_size = 0;
    uint8_t* decrypted = nimcp_decrypt(
        read_back.data(), read_back.size(),
        key1_, sizeof(key1_), &decrypted_size);
    ASSERT_NE(decrypted, nullptr);
    EXPECT_EQ(decrypted_size, data.size());
    EXPECT_EQ(memcmp(decrypted, data.data(), data.size()), 0)
        << "Data decrypted after file round-trip must match original";

    free(encrypted);
    free(decrypted);
    remove(path.c_str());
}

//=============================================================================
// Multi-Buffer Tests
//=============================================================================

TEST_F(SerializationAesIntegrationTest, MultipleBuffersSameKeyIndependentDecrypt) {
    // WHAT: Encrypt N different buffers with the same key, then decrypt each
    //       independently and verify correctness
    // WHY:  Each encryption has its own IV, so buffers must decrypt independently

    if (!nimcp_aes_available()) {
        GTEST_SKIP() << "AES not available";
    }

    const size_t NUM_BUFFERS = 10;
    std::vector<std::vector<uint8_t>> originals(NUM_BUFFERS);
    std::vector<uint8_t*> encrypted_ptrs(NUM_BUFFERS, nullptr);
    std::vector<size_t> encrypted_sizes(NUM_BUFFERS, 0);

    // Encrypt N buffers
    for (size_t i = 0; i < NUM_BUFFERS; i++) {
        originals[i] = make_test_data(64 + i * 32, static_cast<uint8_t>(i));
        encrypted_ptrs[i] = nimcp_encrypt(
            originals[i].data(), originals[i].size(),
            key1_, sizeof(key1_), &encrypted_sizes[i]);
        ASSERT_NE(encrypted_ptrs[i], nullptr) << "Encryption failed for buffer " << i;
    }

    // Decrypt each independently (in reverse order to prove independence)
    for (size_t i = NUM_BUFFERS; i > 0; i--) {
        size_t idx = i - 1;
        size_t decrypted_size = 0;
        uint8_t* decrypted = nimcp_decrypt(
            encrypted_ptrs[idx], encrypted_sizes[idx],
            key1_, sizeof(key1_), &decrypted_size);
        ASSERT_NE(decrypted, nullptr) << "Decryption failed for buffer " << idx;
        EXPECT_EQ(decrypted_size, originals[idx].size());
        EXPECT_EQ(memcmp(decrypted, originals[idx].data(), originals[idx].size()), 0)
            << "Buffer " << idx << " decrypted data must match original";
        free(decrypted);
    }

    // Cleanup
    for (size_t i = 0; i < NUM_BUFFERS; i++) {
        free(encrypted_ptrs[i]);
    }
}

//=============================================================================
// nimcp_write_processed / nimcp_read_processed Integration
//=============================================================================

TEST_F(SerializationAesIntegrationTest, WriteProcessedReadProcessedEncrypted) {
    // WHAT: Use nimcp_write_processed with ENCRYPTED flag, then
    //       nimcp_read_processed to decrypt
    // WHY:  This is the actual code path used by brain persistence

    if (!nimcp_aes_available()) {
        GTEST_SKIP() << "AES not available";
    }

    auto data = make_test_data(256);
    std::string path = make_temp_path("write_processed.bin");

    // Write with encryption flag
    FILE* wf = fopen(path.c_str(), "wb");
    ASSERT_NE(wf, nullptr);
    bool write_ok = nimcp_write_processed(
        wf, data.data(), data.size(),
        NIMCP_FORMAT_FLAG_ENCRYPTED,
        key1_, sizeof(key1_));
    long file_size = ftell(wf);
    fclose(wf);
    ASSERT_TRUE(write_ok) << "nimcp_write_processed with encryption must succeed";
    ASSERT_GT(file_size, 0);

    // Read back with decryption
    FILE* rf = fopen(path.c_str(), "rb");
    ASSERT_NE(rf, nullptr);
    size_t out_size = 0;
    uint8_t* result = nimcp_read_processed(
        rf, NIMCP_FORMAT_FLAG_ENCRYPTED, static_cast<size_t>(file_size),
        key1_, sizeof(key1_), &out_size);
    fclose(rf);
    ASSERT_NE(result, nullptr) << "nimcp_read_processed with decryption must succeed";
    EXPECT_EQ(out_size, data.size());
    EXPECT_EQ(memcmp(result, data.data(), data.size()), 0)
        << "write_processed + read_processed round-trip must preserve data";

    free(result);
    remove(path.c_str());
}

TEST_F(SerializationAesIntegrationTest, WriteProcessedReadProcessedCompressedAndEncrypted) {
    // WHAT: Use nimcp_write_processed with both COMPRESSED and ENCRYPTED flags
    // WHY:  Brain persistence can use both compression and encryption together

    if (!nimcp_aes_available()) {
        GTEST_SKIP() << "AES not available";
    }

    // Use repetitive data that compresses well
    std::vector<uint8_t> data(1024);
    for (size_t i = 0; i < data.size(); i++) {
        data[i] = static_cast<uint8_t>(i % 4);
    }

    std::string path = make_temp_path("compress_encrypt.bin");
    uint32_t flags = NIMCP_FORMAT_FLAG_COMPRESSED | NIMCP_FORMAT_FLAG_ENCRYPTED;

    // Write with both flags
    FILE* wf = fopen(path.c_str(), "wb");
    ASSERT_NE(wf, nullptr);
    bool write_ok = nimcp_write_processed(
        wf, data.data(), data.size(), flags,
        key1_, sizeof(key1_));
    long file_size = ftell(wf);
    fclose(wf);
    ASSERT_TRUE(write_ok) << "Write with compress+encrypt must succeed";
    ASSERT_GT(file_size, 0);

    // Read back with both flags
    FILE* rf = fopen(path.c_str(), "rb");
    ASSERT_NE(rf, nullptr);
    size_t out_size = 0;
    uint8_t* result = nimcp_read_processed(
        rf, flags, static_cast<size_t>(file_size),
        key1_, sizeof(key1_), &out_size);
    fclose(rf);
    ASSERT_NE(result, nullptr) << "Read with decompress+decrypt must succeed";
    EXPECT_EQ(out_size, data.size());
    EXPECT_EQ(memcmp(result, data.data(), data.size()), 0)
        << "Compress+encrypt round-trip must preserve data";

    free(result);
    remove(path.c_str());
}

TEST_F(SerializationAesIntegrationTest, WriteProcessedEncryptedReadWithWrongKeyFails) {
    // WHAT: Write encrypted data, read back with wrong key
    // WHY:  Verify authentication failure propagates through the
    //       nimcp_write_processed/nimcp_read_processed path

    if (!nimcp_aes_available()) {
        GTEST_SKIP() << "AES not available";
    }

    auto data = make_test_data(128);
    std::string path = make_temp_path("wrong_key_processed.bin");

    // Write with key1
    FILE* wf = fopen(path.c_str(), "wb");
    ASSERT_NE(wf, nullptr);
    bool write_ok = nimcp_write_processed(
        wf, data.data(), data.size(),
        NIMCP_FORMAT_FLAG_ENCRYPTED,
        key1_, sizeof(key1_));
    long file_size = ftell(wf);
    fclose(wf);
    ASSERT_TRUE(write_ok);

    // Read with key2 - must fail
    FILE* rf = fopen(path.c_str(), "rb");
    ASSERT_NE(rf, nullptr);
    size_t out_size = 0;
    uint8_t* result = nimcp_read_processed(
        rf, NIMCP_FORMAT_FLAG_ENCRYPTED, static_cast<size_t>(file_size),
        key2_, sizeof(key2_), &out_size);
    fclose(rf);
    EXPECT_EQ(result, nullptr)
        << "Reading encrypted data with wrong key via read_processed must fail";

    if (result) free(result);
    remove(path.c_str());
}

TEST_F(SerializationAesIntegrationTest, ContextKeyTruncationConsistency) {
    // WHAT: Set a 64-byte key via context (which truncates to 32),
    //       encrypt with context key, decrypt with first 32 bytes directly
    // WHY:  Verify context key truncation is consistent with direct API usage

    if (!nimcp_aes_available()) {
        GTEST_SKIP() << "AES not available";
    }

    // 64-byte key
    uint8_t long_key[64];
    for (size_t i = 0; i < 64; i++) {
        long_key[i] = static_cast<uint8_t>(0x10 + i);
    }

    // Context truncates to 32 bytes
    nimcp_serialize_ctx_t ctx;
    nimcp_serialize_ctx_init(&ctx);
    nimcp_serialize_ctx_set_key(&ctx, long_key, sizeof(long_key));
    EXPECT_EQ(ctx.key_size, 32u);

    uint8_t data[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    // Encrypt with context key (truncated to 32)
    size_t encrypted_size = 0;
    uint8_t* encrypted = nimcp_encrypt(
        data, sizeof(data),
        ctx.key, ctx.key_size, &encrypted_size);
    ASSERT_NE(encrypted, nullptr);

    // Decrypt with first 32 bytes of the original long key
    size_t decrypted_size = 0;
    uint8_t* decrypted = nimcp_decrypt(
        encrypted, encrypted_size,
        long_key, 32, &decrypted_size);
    ASSERT_NE(decrypted, nullptr)
        << "Decrypt with first 32 bytes must match context-truncated key";
    EXPECT_EQ(decrypted_size, sizeof(data));
    EXPECT_EQ(memcmp(decrypted, data, sizeof(data)), 0);

    free(encrypted);
    free(decrypted);
}

TEST_F(SerializationAesIntegrationTest, LargeFilePersistenceRoundTrip) {
    // WHAT: Encrypt a large (256KB) payload, write to file, read back, decrypt
    // WHY:  Realistic brain state sizes need to survive file persistence

    if (!nimcp_aes_available()) {
        GTEST_SKIP() << "AES not available";
    }

    const size_t SIZE = 256 * 1024;  // 256 KB
    auto data = make_test_data(SIZE);
    std::string path = make_temp_path("large_roundtrip.bin");

    // Encrypt
    size_t encrypted_size = 0;
    uint8_t* encrypted = nimcp_encrypt(
        data.data(), data.size(),
        key1_, sizeof(key1_), &encrypted_size);
    ASSERT_NE(encrypted, nullptr);

    // Write to file
    FILE* wf = fopen(path.c_str(), "wb");
    ASSERT_NE(wf, nullptr);
    ASSERT_EQ(fwrite(encrypted, 1, encrypted_size, wf), encrypted_size);
    fclose(wf);

    // Read back
    FILE* rf = fopen(path.c_str(), "rb");
    ASSERT_NE(rf, nullptr);
    std::vector<uint8_t> file_data(encrypted_size);
    ASSERT_EQ(fread(file_data.data(), 1, encrypted_size, rf), encrypted_size);
    fclose(rf);

    // Decrypt
    size_t decrypted_size = 0;
    uint8_t* decrypted = nimcp_decrypt(
        file_data.data(), file_data.size(),
        key1_, sizeof(key1_), &decrypted_size);
    ASSERT_NE(decrypted, nullptr);
    EXPECT_EQ(decrypted_size, data.size());
    EXPECT_EQ(memcmp(decrypted, data.data(), data.size()), 0)
        << "256KB payload must survive file persistence round-trip";

    free(encrypted);
    free(decrypted);
    remove(path.c_str());
}

} // namespace
