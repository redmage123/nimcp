/**
 * @file test_serialization_aes_e2e.cpp
 * @brief End-to-end tests for AES-256-GCM encryption in nimcp_serialization
 *
 * WHAT: Full workflow tests simulating real-world usage: create context,
 *       configure, encrypt, persist, load, decrypt, verify
 * WHY:  Validate the complete user journey through the encryption API
 * HOW:  Simulate key rotation, stress-test with 1000 cycles, and test
 *       the full file-based persistence workflow
 *
 * @date 2026-02-12
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <vector>
#include <string>
#include <unistd.h>

// Header has its own extern "C" guard
#include "utils/serialization/nimcp_serialization.h"

// Format flag constants
#define NIMCP_FORMAT_FLAG_COMPRESSED  0x00000001
#define NIMCP_FORMAT_FLAG_ENCRYPTED   0x00000002

namespace {

//=============================================================================
// Test Fixture
//=============================================================================

class SerializationAesE2ETest : public ::testing::Test {
protected:
    std::string make_temp_path(const char* suffix) {
        char path[256];
        snprintf(path, sizeof(path), "/tmp/nimcp_aes_e2e_%d_%s",
                 static_cast<int>(getpid()), suffix);
        return std::string(path);
    }

    // Helper: generate deterministic data with specific content
    std::vector<uint8_t> make_sensitive_data(const char* content) {
        size_t len = strlen(content) + 1;
        std::vector<uint8_t> data(len);
        memcpy(data.data(), content, len);
        return data;
    }

    // Helper: generate random-looking data with LCG
    std::vector<uint8_t> make_random_data(size_t size, uint32_t seed) {
        std::vector<uint8_t> data(size);
        uint32_t state = seed;
        for (size_t i = 0; i < size; i++) {
            state = state * 1103515245u + 12345u;
            data[i] = static_cast<uint8_t>((state >> 16) & 0xFF);
        }
        return data;
    }
};

//=============================================================================
// Full Workflow E2E
//=============================================================================

TEST_F(SerializationAesE2ETest, FullWorkflowContextEncryptPersistDecrypt) {
    // WHAT: Complete user workflow:
    //       1. Create serialization context
    //       2. Set encryption key
    //       3. Encrypt sensitive data
    //       4. Write encrypted data to temp file
    //       5. Read encrypted data back from file
    //       6. Decrypt with same context
    //       7. Verify data matches original
    // WHY:  This is the exact sequence a brain persistence save/load uses

    if (!nimcp_aes_available()) {
        GTEST_SKIP() << "AES not available";
    }

    // Step 1: Create context
    nimcp_serialize_ctx_t ctx;
    nimcp_serialize_ctx_init(&ctx);
    ASSERT_FALSE(ctx.key_set);
    ASSERT_EQ(ctx.flags, 0u);

    // Step 2: Set encryption key
    uint8_t master_key[32];
    for (size_t i = 0; i < 32; i++) {
        master_key[i] = static_cast<uint8_t>(0xA0 + i);
    }
    nimcp_serialize_ctx_set_key(&ctx, master_key, sizeof(master_key));
    ASSERT_TRUE(ctx.key_set);
    ASSERT_EQ(ctx.key_size, 32u);

    // Set encryption flag
    nimcp_serialize_ctx_set_flags(&ctx, NIMCP_FORMAT_FLAG_ENCRYPTED);
    ASSERT_EQ(ctx.flags, NIMCP_FORMAT_FLAG_ENCRYPTED);

    // Step 3: Encrypt sensitive data
    auto sensitive = make_sensitive_data(
        "NIMCP brain state: weights=[0.5, 0.3, 0.8], biases=[0.1, -0.2]");
    size_t encrypted_size = 0;
    uint8_t* encrypted = nimcp_encrypt(
        sensitive.data(), sensitive.size(),
        ctx.key, ctx.key_size, &encrypted_size);
    ASSERT_NE(encrypted, nullptr) << "Encryption must succeed";
    ASSERT_GT(encrypted_size, sensitive.size())
        << "Encrypted data must be larger than plaintext (IV + tag overhead)";

    // Verify encrypted data does not contain the plaintext
    bool found_plaintext = false;
    if (encrypted_size >= sensitive.size()) {
        for (size_t i = 0; i <= encrypted_size - sensitive.size(); i++) {
            if (memcmp(encrypted + i, sensitive.data(), sensitive.size()) == 0) {
                found_plaintext = true;
                break;
            }
        }
    }
    EXPECT_FALSE(found_plaintext)
        << "Encrypted data must not contain plaintext substring";

    // Step 4: Write encrypted data to temp file
    std::string path = make_temp_path("full_workflow.enc");
    FILE* wf = fopen(path.c_str(), "wb");
    ASSERT_NE(wf, nullptr) << "Failed to create temp file";
    size_t written = fwrite(encrypted, 1, encrypted_size, wf);
    fclose(wf);
    ASSERT_EQ(written, encrypted_size) << "File write must complete fully";

    // Step 5: Read encrypted data back
    FILE* rf = fopen(path.c_str(), "rb");
    ASSERT_NE(rf, nullptr) << "Failed to open temp file for reading";
    fseek(rf, 0, SEEK_END);
    long file_size = ftell(rf);
    fseek(rf, 0, SEEK_SET);
    ASSERT_EQ(static_cast<size_t>(file_size), encrypted_size)
        << "File size must match encrypted data size";

    std::vector<uint8_t> file_content(file_size);
    size_t read_count = fread(file_content.data(), 1, file_size, rf);
    fclose(rf);
    ASSERT_EQ(read_count, static_cast<size_t>(file_size));

    // Step 6: Decrypt with same context key
    size_t decrypted_size = 0;
    uint8_t* decrypted = nimcp_decrypt(
        file_content.data(), file_content.size(),
        ctx.key, ctx.key_size, &decrypted_size);
    ASSERT_NE(decrypted, nullptr) << "Decryption must succeed";

    // Step 7: Verify data matches original
    EXPECT_EQ(decrypted_size, sensitive.size());
    EXPECT_EQ(memcmp(decrypted, sensitive.data(), sensitive.size()), 0)
        << "Decrypted data must exactly match original sensitive data";

    // Cleanup
    free(encrypted);
    free(decrypted);
    remove(path.c_str());
}

//=============================================================================
// Key Rotation E2E
//=============================================================================

TEST_F(SerializationAesE2ETest, KeyRotationSimulation) {
    // WHAT: Simulate key rotation:
    //       1. Encrypt data with key1
    //       2. Verify decryption FAILS with key2 (new key)
    //       3. Verify decryption SUCCEEDS with key1 (original key)
    //       4. Re-encrypt with key2 for migration
    //       5. Verify decryption succeeds with key2
    //       6. Verify decryption fails with key1 on re-encrypted data
    // WHY:  Key rotation is a common security practice; the system must
    //       support it cleanly

    if (!nimcp_aes_available()) {
        GTEST_SKIP() << "AES not available";
    }

    // Two different keys
    uint8_t key1[32], key2[32];
    for (size_t i = 0; i < 32; i++) {
        key1[i] = static_cast<uint8_t>(0x11 + i);
        key2[i] = static_cast<uint8_t>(0x99 + i);
    }

    auto data = make_sensitive_data("Classified brain model parameters v3.7");

    // Step 1: Encrypt with key1
    size_t enc1_size = 0;
    uint8_t* enc1 = nimcp_encrypt(
        data.data(), data.size(), key1, 32, &enc1_size);
    ASSERT_NE(enc1, nullptr);

    // Step 2: Decrypt with key2 must FAIL
    size_t dec_fail_size = 0;
    uint8_t* dec_fail = nimcp_decrypt(
        enc1, enc1_size, key2, 32, &dec_fail_size);
    EXPECT_EQ(dec_fail, nullptr)
        << "Decryption with new key (key2) on data encrypted with key1 must fail";
    if (dec_fail) free(dec_fail);

    // Step 3: Decrypt with key1 must SUCCEED
    size_t dec1_size = 0;
    uint8_t* dec1 = nimcp_decrypt(
        enc1, enc1_size, key1, 32, &dec1_size);
    ASSERT_NE(dec1, nullptr) << "Decryption with original key1 must succeed";
    EXPECT_EQ(dec1_size, data.size());
    EXPECT_EQ(memcmp(dec1, data.data(), data.size()), 0);

    // Step 4: Re-encrypt the decrypted data with key2 (migration)
    size_t enc2_size = 0;
    uint8_t* enc2 = nimcp_encrypt(
        dec1, dec1_size, key2, 32, &enc2_size);
    ASSERT_NE(enc2, nullptr) << "Re-encryption with key2 must succeed";

    // Step 5: Decrypt with key2 must SUCCEED
    size_t dec2_size = 0;
    uint8_t* dec2 = nimcp_decrypt(
        enc2, enc2_size, key2, 32, &dec2_size);
    ASSERT_NE(dec2, nullptr) << "Decryption with key2 after re-encryption must succeed";
    EXPECT_EQ(dec2_size, data.size());
    EXPECT_EQ(memcmp(dec2, data.data(), data.size()), 0)
        << "Re-encrypted data must decrypt to original plaintext";

    // Step 6: Decrypt re-encrypted data with key1 must FAIL
    size_t dec_old_size = 0;
    uint8_t* dec_old = nimcp_decrypt(
        enc2, enc2_size, key1, 32, &dec_old_size);
    EXPECT_EQ(dec_old, nullptr)
        << "Old key1 must not decrypt data re-encrypted with key2";

    free(enc1);
    free(dec1);
    free(enc2);
    free(dec2);
    if (dec_old) free(dec_old);
}

//=============================================================================
// Stress Test
//=============================================================================

TEST_F(SerializationAesE2ETest, StressTest1000Cycles) {
    // WHAT: Perform 1000 encrypt/decrypt cycles with varying payload sizes
    // WHY:  Stress test for memory leaks, accumulated state, and correctness
    //       under sustained load

    if (!nimcp_aes_available()) {
        GTEST_SKIP() << "AES not available";
    }

    uint8_t key[32];
    for (size_t i = 0; i < 32; i++) {
        key[i] = static_cast<uint8_t>(0x55 + i);
    }

    const int NUM_CYCLES = 1000;
    int success_count = 0;

    for (int cycle = 0; cycle < NUM_CYCLES; cycle++) {
        // Vary payload size: 1 byte to 8KB
        size_t payload_size = 1 + (static_cast<size_t>(cycle) * 8191) % 8192;
        auto original = make_random_data(payload_size,
            static_cast<uint32_t>(cycle * 6271));

        // Encrypt
        size_t encrypted_size = 0;
        uint8_t* encrypted = nimcp_encrypt(
            original.data(), original.size(),
            key, sizeof(key), &encrypted_size);
        if (!encrypted) {
            ADD_FAILURE() << "Encryption failed at cycle " << cycle
                          << " (size=" << payload_size << ")";
            continue;
        }

        // Decrypt
        size_t decrypted_size = 0;
        uint8_t* decrypted = nimcp_decrypt(
            encrypted, encrypted_size,
            key, sizeof(key), &decrypted_size);
        if (!decrypted) {
            ADD_FAILURE() << "Decryption failed at cycle " << cycle
                          << " (size=" << payload_size << ")";
            free(encrypted);
            continue;
        }

        // Verify
        if (decrypted_size == original.size() &&
            memcmp(decrypted, original.data(), original.size()) == 0) {
            success_count++;
        } else {
            ADD_FAILURE() << "Data mismatch at cycle " << cycle
                          << " (size=" << payload_size << ")"
                          << " decrypted_size=" << decrypted_size
                          << " expected=" << original.size();
        }

        free(encrypted);
        free(decrypted);
    }

    EXPECT_EQ(success_count, NUM_CYCLES)
        << "All " << NUM_CYCLES << " encrypt/decrypt cycles must succeed";
}

TEST_F(SerializationAesE2ETest, WriteProcessedFullPersistenceWorkflow) {
    // WHAT: Full persistence workflow using nimcp_write_processed and
    //       nimcp_read_processed - the actual API path used by brain save/load
    // WHY:  E2E validation of the high-level API, not just raw encrypt/decrypt

    if (!nimcp_aes_available()) {
        GTEST_SKIP() << "AES not available";
    }

    // Configure context
    nimcp_serialize_ctx_t ctx;
    nimcp_serialize_ctx_init(&ctx);
    uint8_t key[32];
    for (size_t i = 0; i < 32; i++) key[i] = static_cast<uint8_t>(0xC0 + i);
    nimcp_serialize_ctx_set_key(&ctx, key, sizeof(key));
    nimcp_serialize_ctx_set_flags(&ctx, NIMCP_FORMAT_FLAG_ENCRYPTED);

    // Prepare realistic brain-state-like data
    const size_t DATA_SIZE = 4096;
    auto brain_state = make_random_data(DATA_SIZE, 0xBEEF);

    std::string path = make_temp_path("e2e_persistence.dat");

    // SAVE: write_processed with encryption
    FILE* wf = fopen(path.c_str(), "wb");
    ASSERT_NE(wf, nullptr);
    bool write_ok = nimcp_write_processed(
        wf, brain_state.data(), brain_state.size(),
        ctx.flags, ctx.key, ctx.key_size);
    long file_size = ftell(wf);
    fclose(wf);
    ASSERT_TRUE(write_ok) << "nimcp_write_processed must succeed";
    ASSERT_GT(file_size, 0);

    // LOAD: read_processed with decryption
    FILE* rf = fopen(path.c_str(), "rb");
    ASSERT_NE(rf, nullptr);
    size_t loaded_size = 0;
    uint8_t* loaded = nimcp_read_processed(
        rf, ctx.flags, static_cast<size_t>(file_size),
        ctx.key, ctx.key_size, &loaded_size);
    fclose(rf);

    ASSERT_NE(loaded, nullptr) << "nimcp_read_processed must succeed";
    EXPECT_EQ(loaded_size, brain_state.size());
    EXPECT_EQ(memcmp(loaded, brain_state.data(), brain_state.size()), 0)
        << "Loaded brain state must match original exactly";

    free(loaded);
    remove(path.c_str());
}

TEST_F(SerializationAesE2ETest, MultipleFilesWithDifferentKeys) {
    // WHAT: Save multiple encrypted files with different keys, then load each
    //       with the correct key and verify cross-key access fails
    // WHY:  Simulates a multi-model scenario where each brain has its own key

    if (!nimcp_aes_available()) {
        GTEST_SKIP() << "AES not available";
    }

    const int NUM_FILES = 5;
    uint8_t keys[NUM_FILES][32];
    std::vector<std::vector<uint8_t>> originals(NUM_FILES);
    std::vector<std::string> paths(NUM_FILES);

    // Create and save NUM_FILES encrypted files
    for (int f = 0; f < NUM_FILES; f++) {
        // Unique key per file
        for (size_t i = 0; i < 32; i++) {
            keys[f][i] = static_cast<uint8_t>((f + 1) * 0x10 + i);
        }

        originals[f] = make_random_data(128 + f * 64,
            static_cast<uint32_t>(f * 9973));

        size_t enc_size = 0;
        uint8_t* enc = nimcp_encrypt(
            originals[f].data(), originals[f].size(),
            keys[f], 32, &enc_size);
        ASSERT_NE(enc, nullptr);

        char suffix[32];
        snprintf(suffix, sizeof(suffix), "multi_%d.enc", f);
        paths[f] = make_temp_path(suffix);

        FILE* wf = fopen(paths[f].c_str(), "wb");
        ASSERT_NE(wf, nullptr);
        fwrite(enc, 1, enc_size, wf);
        fclose(wf);
        free(enc);
    }

    // Load each file with correct key
    for (int f = 0; f < NUM_FILES; f++) {
        FILE* rf = fopen(paths[f].c_str(), "rb");
        ASSERT_NE(rf, nullptr);
        fseek(rf, 0, SEEK_END);
        long fsize = ftell(rf);
        fseek(rf, 0, SEEK_SET);

        std::vector<uint8_t> file_data(fsize);
        fread(file_data.data(), 1, fsize, rf);
        fclose(rf);

        // Correct key succeeds
        size_t dec_size = 0;
        uint8_t* dec = nimcp_decrypt(
            file_data.data(), file_data.size(),
            keys[f], 32, &dec_size);
        ASSERT_NE(dec, nullptr) << "File " << f << " decrypt with correct key must succeed";
        EXPECT_EQ(dec_size, originals[f].size());
        EXPECT_EQ(memcmp(dec, originals[f].data(), originals[f].size()), 0);
        free(dec);

        // Wrong key (next file's key) fails
        int wrong_idx = (f + 1) % NUM_FILES;
        size_t fail_size = 0;
        uint8_t* fail = nimcp_decrypt(
            file_data.data(), file_data.size(),
            keys[wrong_idx], 32, &fail_size);
        EXPECT_EQ(fail, nullptr)
            << "File " << f << " decrypt with wrong key (key " << wrong_idx << ") must fail";
        if (fail) free(fail);
    }

    // Cleanup
    for (int f = 0; f < NUM_FILES; f++) {
        remove(paths[f].c_str());
    }
}

} // namespace
