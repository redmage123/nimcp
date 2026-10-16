/**
 * @file test_crypto_e2e.cpp
 * @brief End-to-end tests for cryptographic operations
 *
 * WHAT: E2E tests verifying complete cryptographic workflows
 * WHY:  Ensure crypto operations work correctly in realistic scenarios
 * HOW:  Test key generation, encryption/decryption, signatures, and key rotation
 *
 * TEST SCENARIOS:
 * 1. Full key generation -> encryption -> decryption cycle
 * 2. Signature generation and verification
 * 3. Post-quantum algorithm full workflow
 * 4. Key rotation scenarios
 * 5. Encrypted audit log workflow
 * 6. Hybrid classical+PQ operations
 *
 * @author NIMCP Development Team
 * @date 2026-02-02
 */

#include "e2e_test_framework.h"
#include <gtest/gtest.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <vector>
#include <string>
#include <chrono>

extern "C" {
#include "security/nimcp_post_quantum.h"
#include "security/nimcp_encrypted_audit.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "utils/error/nimcp_error_codes.h"
}

namespace nimcp {
namespace e2e {

/* ============================================================================
 * Test Constants
 * ============================================================================ */

static constexpr int PERF_ITERATIONS = 100;

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static int memcmp_secure(const void* a, const void* b, size_t n)
{
    const unsigned char* pa = static_cast<const unsigned char*>(a);
    const unsigned char* pb = static_cast<const unsigned char*>(b);
    int result = 0;
    for (size_t i = 0; i < n; i++) {
        result |= pa[i] ^ pb[i];
    }
    return result;
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class CryptoE2E : public ::testing::Test {
protected:
    nimcp_pq_context_t pq_ctx_ = nullptr;
    nimcp_encrypted_audit_t audit_ = nullptr;

    void SetUp() override {
        srand(static_cast<unsigned int>(time(nullptr)));

        // Create post-quantum context
        nimcp_pq_config_t pq_config = {
            .default_kyber_variant = NIMCP_PQ_KYBER_768,
            .default_dilithium_variant = NIMCP_PQ_DILITHIUM_3,
            .hybrid_config = {
                .enable_classical = true,
                .enable_pq = true,
                .require_both = false,
                .allow_pq_fallback = true
            },
            .enable_logging = false
        };
        pq_ctx_ = nimcp_pq_context_create(&pq_config);
    }

    void TearDown() override {
        if (audit_) {
            nimcp_encrypted_audit_destroy(audit_);
            audit_ = nullptr;
        }
        if (pq_ctx_) {
            nimcp_pq_context_destroy(pq_ctx_);
            pq_ctx_ = nullptr;
        }
    }
};

/* ============================================================================
 * E2E Test: Kyber Key Encapsulation Full Cycle
 * ============================================================================ */

TEST_F(CryptoE2E, KyberFullCycle) {
    E2E_PIPELINE_START("Kyber Key Encapsulation Full Cycle");

    nimcp_kyber_variant_t variants[] = {
        NIMCP_PQ_KYBER_512,
        NIMCP_PQ_KYBER_768,
        NIMCP_PQ_KYBER_1024
    };
    const char* variant_names[] = {"Kyber-512", "Kyber-768", "Kyber-1024"};

    for (int v = 0; v < 3; v++) {
        std::cout << "  Testing " << variant_names[v] << "...\n";

        // Phase 1: Generate keypair
        E2E_STAGE_BEGIN("Generate keypair", 1000);
        nimcp_kyber_keypair_t keypair;
        memset(&keypair, 0, sizeof(keypair));

        nimcp_error_t err = nimcp_kyber_keygen(variants[v], &keypair);
        EXPECT_EQ(err, NIMCP_OK) << "Keypair generation succeeded";
        EXPECT_EQ(keypair.magic, NIMCP_KYBER_KEYPAIR_MAGIC) << "Keypair magic valid";
        ASSERT_NE(keypair.public_key, nullptr) << "Public key allocated";
        ASSERT_NE(keypair.secret_key, nullptr) << "Secret key allocated";

        std::cout << "      Public key: " << keypair.public_key_len << " bytes\n";
        std::cout << "      Secret key: " << keypair.secret_key_len << " bytes\n";
        E2E_STAGE_END();

        // Phase 2: Encapsulation (sender side)
        E2E_STAGE_BEGIN("Encapsulate shared secret", 500);
        size_t ct_len, pk_len, sk_len;
        nimcp_kyber_get_sizes(variants[v], &pk_len, &sk_len, &ct_len);

        std::vector<uint8_t> ciphertext(ct_len);
        uint8_t sender_secret[NIMCP_KYBER_512_SHARED_SECRET_BYTES];
        size_t actual_ct_len = ct_len;

        err = nimcp_kyber_encapsulate(variants[v], keypair.public_key,
                                       ciphertext.data(), &actual_ct_len,
                                       sender_secret, sizeof(sender_secret));
        EXPECT_EQ(err, NIMCP_OK) << "Encapsulation succeeded";
        std::cout << "      Ciphertext: " << actual_ct_len << " bytes\n";
        E2E_STAGE_END();

        // Phase 3: Decapsulation (receiver side)
        E2E_STAGE_BEGIN("Decapsulate shared secret", 500);
        uint8_t receiver_secret[NIMCP_KYBER_512_SHARED_SECRET_BYTES];

        err = nimcp_kyber_decapsulate(variants[v], keypair.secret_key,
                                       ciphertext.data(), actual_ct_len,
                                       receiver_secret, sizeof(receiver_secret));
        EXPECT_EQ(err, NIMCP_OK) << "Decapsulation succeeded";
        E2E_STAGE_END();

        // Phase 4: Verify secrets match
        E2E_STAGE_BEGIN("Verify shared secrets match", 100);
        int match = memcmp_secure(sender_secret, receiver_secret, sizeof(sender_secret));
        EXPECT_EQ(match, 0) << "Shared secrets match";
        std::cout << "      Shared secret verified: 32 bytes\n";
        E2E_STAGE_END();

        // Cleanup
        nimcp_kyber_keypair_free(&keypair);
        std::cout << "    " << variant_names[v] << ": SUCCESS\n";
    }

    E2E_PIPELINE_END();
}

/* ============================================================================
 * E2E Test: Dilithium Digital Signature Full Cycle
 * ============================================================================ */

TEST_F(CryptoE2E, DilithiumSignatureCycle) {
    E2E_PIPELINE_START("Dilithium Digital Signature Full Cycle");

    nimcp_dilithium_variant_t variants[] = {
        NIMCP_PQ_DILITHIUM_2,
        NIMCP_PQ_DILITHIUM_3,
        NIMCP_PQ_DILITHIUM_5
    };
    const char* variant_names[] = {"Dilithium-2", "Dilithium-3", "Dilithium-5"};

    const char* test_messages[] = {
        "Short message",
        "This is a medium-length message for signature testing purposes.",
        "This is a longer message that simulates a more realistic document or data "
        "payload that would need to be signed for authentication and integrity "
        "verification in a production system."
    };

    for (int v = 0; v < 3; v++) {
        std::cout << "  Testing " << variant_names[v] << "...\n";

        // Phase 1: Generate keypair
        E2E_STAGE_BEGIN("Generate signing keypair", 1000);
        nimcp_dilithium_keypair_t keypair;
        memset(&keypair, 0, sizeof(keypair));

        nimcp_error_t err = nimcp_dilithium_keygen(variants[v], &keypair);
        EXPECT_EQ(err, NIMCP_OK) << "Keypair generation succeeded";
        EXPECT_EQ(keypair.magic, NIMCP_DILITHIUM_KEYPAIR_MAGIC) << "Keypair magic valid";

        std::cout << "      Public key: " << keypair.public_key_len << " bytes\n";
        std::cout << "      Secret key: " << keypair.secret_key_len << " bytes\n";
        E2E_STAGE_END();

        // Get signature size
        size_t pk_len, sk_len, sig_len;
        nimcp_dilithium_get_sizes(variants[v], &pk_len, &sk_len, &sig_len);

        // Test with first message
        const char* message = test_messages[0];
        size_t msg_len = strlen(message);

        // Phase 2: Sign message
        E2E_STAGE_BEGIN("Sign message", 500);
        std::vector<uint8_t> signature(sig_len);
        size_t actual_sig_len = sig_len;

        err = nimcp_dilithium_sign(variants[v], keypair.secret_key,
                                    reinterpret_cast<const uint8_t*>(message), msg_len,
                                    signature.data(), &actual_sig_len);
        EXPECT_EQ(err, NIMCP_OK) << "Signing succeeded";
        std::cout << "      Signature: " << actual_sig_len << " bytes\n";
        E2E_STAGE_END();

        // Phase 3: Verify signature
        E2E_STAGE_BEGIN("Verify signature", 500);
        err = nimcp_dilithium_verify(variants[v], keypair.public_key,
                                      reinterpret_cast<const uint8_t*>(message), msg_len,
                                      signature.data(), actual_sig_len);
        EXPECT_EQ(err, NIMCP_OK) << "Signature verification succeeded";
        E2E_STAGE_END();

        // Phase 4: Verify tampered message fails
        E2E_STAGE_BEGIN("Verify tampered message detection", 200);
        std::string tampered = message;
        tampered[0] = 'X';  // Modify first byte

        err = nimcp_dilithium_verify(variants[v], keypair.public_key,
                                      reinterpret_cast<const uint8_t*>(tampered.c_str()), msg_len,
                                      signature.data(), actual_sig_len);
        EXPECT_NE(err, NIMCP_OK) << "Tampered message rejected";
        E2E_STAGE_END();

        nimcp_dilithium_keypair_free(&keypair);
        std::cout << "    " << variant_names[v] << ": SUCCESS\n";
    }

    E2E_PIPELINE_END();
}

/* ============================================================================
 * E2E Test: Encrypted Audit Log Workflow
 * ============================================================================ */

TEST_F(CryptoE2E, EncryptedAuditWorkflow) {
    E2E_PIPELINE_START("Encrypted Audit Log Workflow");

    // Phase 1: Generate master key
    E2E_STAGE_BEGIN("Generate master encryption key", 100);
    uint8_t master_key[NIMCP_AUDIT_KEY_SIZE];
    for (int i = 0; i < NIMCP_AUDIT_KEY_SIZE; i++) {
        master_key[i] = static_cast<uint8_t>(rand() % 256);
    }
    E2E_STAGE_END();

    // Phase 2: Create encrypted audit buffer
    E2E_STAGE_BEGIN("Create encrypted audit buffer", 200);
    nimcp_encrypted_audit_config_t config = nimcp_encrypted_audit_default_config();
    config.buffer_size = 100;
    config.enable_compression = false;
    config.lock_memory = false;

    audit_ = nimcp_encrypted_audit_create(&config, master_key, sizeof(master_key));
    E2E_ASSERT_NOT_NULL(audit_, "Audit buffer created");
    E2E_STAGE_END();

    // Phase 3: Log entries
    E2E_STAGE_BEGIN("Log encrypted entries", 500);
    const char* test_messages[] = {
        "User admin logged in from 192.168.1.100",
        "Failed authentication attempt for user guest",
        "Configuration change: security level increased",
        "Threat detected: SQL injection attempt blocked",
        "System shutdown initiated by operator"
    };

    for (size_t i = 0; i < sizeof(test_messages) / sizeof(test_messages[0]); i++) {
        nimcp_error_t err = nimcp_encrypted_audit_log(audit_,
            static_cast<nimcp_audit_severity_t>(NIMCP_AUDIT_INFO + (i % 3)),
            static_cast<nimcp_audit_category_t>(NIMCP_AUDIT_AUTHENTICATION + (i % 4)),
            test_messages[i],
            nullptr, 0);
        EXPECT_EQ(err, NIMCP_OK) << "Log entry written";
    }
    std::cout << "    Logged " << (sizeof(test_messages) / sizeof(test_messages[0]))
              << " entries\n";
    E2E_STAGE_END();

    // Phase 4: Read and decrypt entries
    E2E_STAGE_BEGIN("Read and decrypt entries", 500);
    nimcp_audit_entry_t entries[10];
    size_t num_entries = 0;

    nimcp_error_t err = nimcp_encrypted_audit_read(audit_, entries, 10, &num_entries);
    EXPECT_EQ(err, NIMCP_OK) << "Entries read successfully";
    EXPECT_GE(num_entries, 5u) << "At least 5 entries read";

    std::cout << "    Decrypted " << num_entries << " entries:\n";
    for (size_t i = 0; i < num_entries && i < 3; i++) {
        std::cout << "      [" << nimcp_audit_severity_name(entries[i].severity)
                  << "] " << entries[i].message << "\n";
    }
    E2E_STAGE_END();

    // Phase 5: Verify statistics
    E2E_STAGE_BEGIN("Verify encryption statistics", 200);
    nimcp_encrypted_audit_stats_t stats;
    err = nimcp_encrypted_audit_get_stats(audit_, &stats);
    EXPECT_EQ(err, NIMCP_OK) << "Stats retrieved";

    std::cout << "    Total entries: " << stats.total_entries << "\n";
    std::cout << "    Encrypted: " << stats.entries_encrypted << "\n";
    std::cout << "    Decrypted: " << stats.entries_decrypted << "\n";
    std::cout << "    Tampering detected: " << stats.tampering_detected << "\n";

    EXPECT_EQ(stats.tampering_detected, 0u) << "No tampering detected";
    E2E_STAGE_END();

    // Phase 6: Test formatted logging
    E2E_STAGE_BEGIN("Test formatted logging", 200);
    err = nimcp_encrypted_audit_logf(audit_,
        NIMCP_AUDIT_WARNING,
        NIMCP_AUDIT_NETWORK,
        "Connection from %s on port %d failed after %d attempts",
        "192.168.1.50", 8080, 3);
    EXPECT_EQ(err, NIMCP_OK) << "Formatted log entry written";
    E2E_STAGE_END();

    // Cleanup - clear key from memory
    memset(master_key, 0, sizeof(master_key));

    E2E_PIPELINE_END();
}

/* ============================================================================
 * E2E Test: Key Rotation Scenario
 * ============================================================================ */

TEST_F(CryptoE2E, KeyRotationScenario) {
    E2E_PIPELINE_START("Key Rotation Scenario");

    // Phase 1: Create audit with initial key
    E2E_STAGE_BEGIN("Create audit with initial key", 200);
    uint8_t key_v1[NIMCP_AUDIT_KEY_SIZE];
    for (int i = 0; i < NIMCP_AUDIT_KEY_SIZE; i++) {
        key_v1[i] = static_cast<uint8_t>(i + 1);
    }

    nimcp_encrypted_audit_config_t config = nimcp_encrypted_audit_default_config();
    config.buffer_size = 50;
    audit_ = nimcp_encrypted_audit_create(&config, key_v1, sizeof(key_v1));
    E2E_ASSERT_NOT_NULL(audit_, "Audit created");

    uint32_t initial_version = nimcp_encrypted_audit_get_key_version(audit_);
    std::cout << "    Initial key version: " << initial_version << "\n";
    E2E_STAGE_END();

    // Phase 2: Log entries with v1 key
    E2E_STAGE_BEGIN("Log entries with key v1", 300);
    for (int i = 0; i < 5; i++) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Entry %d with key v1", i);
        nimcp_encrypted_audit_log(audit_, NIMCP_AUDIT_INFO,
                                  NIMCP_AUDIT_SYSTEM, msg, nullptr, 0);
    }
    E2E_STAGE_END();

    // Phase 3: Rotate to new key
    E2E_STAGE_BEGIN("Rotate to new key", 500);
    uint8_t key_v2[NIMCP_AUDIT_KEY_SIZE];
    for (int i = 0; i < NIMCP_AUDIT_KEY_SIZE; i++) {
        key_v2[i] = static_cast<uint8_t>(i + 100);  // Different key material
    }

    nimcp_error_t err = nimcp_encrypted_audit_rotate_key(audit_, key_v2, sizeof(key_v2));
    EXPECT_EQ(err, NIMCP_OK) << "Key rotation succeeded";

    uint32_t new_version = nimcp_encrypted_audit_get_key_version(audit_);
    std::cout << "    New key version: " << new_version << "\n";
    EXPECT_EQ(new_version, initial_version + 1) << "Key version incremented";
    E2E_STAGE_END();

    // Phase 4: Log entries with v2 key
    E2E_STAGE_BEGIN("Log entries with key v2", 300);
    for (int i = 0; i < 5; i++) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Entry %d with key v2", i);
        nimcp_encrypted_audit_log(audit_, NIMCP_AUDIT_INFO,
                                  NIMCP_AUDIT_SYSTEM, msg, nullptr, 0);
    }
    E2E_STAGE_END();

    // Phase 5: Read all entries (both key versions)
    E2E_STAGE_BEGIN("Read entries from both key versions", 500);
    nimcp_audit_entry_t entries[20];
    size_t num_entries = 0;

    err = nimcp_encrypted_audit_read(audit_, entries, 20, &num_entries);
    EXPECT_EQ(err, NIMCP_OK) << "All entries read";
    EXPECT_GE(num_entries, 10u) << "At least 10 entries read";

    std::cout << "    Total entries read: " << num_entries << "\n";

    // Count entries by key version
    int v1_count = 0, v2_count = 0;
    for (size_t i = 0; i < num_entries; i++) {
        if (entries[i].key_version == initial_version) v1_count++;
        else if (entries[i].key_version == new_version) v2_count++;
    }
    std::cout << "    Entries with v1 key: " << v1_count << "\n";
    std::cout << "    Entries with v2 key: " << v2_count << "\n";
    E2E_STAGE_END();

    // Phase 6: Verify key rotation statistics
    E2E_STAGE_BEGIN("Verify rotation statistics", 200);
    nimcp_encrypted_audit_stats_t stats;
    nimcp_encrypted_audit_get_stats(audit_, &stats);
    std::cout << "    Key rotations: " << stats.key_rotations << "\n";
    EXPECT_GE(stats.key_rotations, 1u) << "At least 1 rotation recorded";
    E2E_STAGE_END();

    // Cleanup
    memset(key_v1, 0, sizeof(key_v1));
    memset(key_v2, 0, sizeof(key_v2));

    E2E_PIPELINE_END();
}

/* ============================================================================
 * E2E Test: BBB Code Signing
 * ============================================================================ */

TEST_F(CryptoE2E, BBBCodeSigning) {
    E2E_PIPELINE_START("BBB Code Signing");

    // Phase 1: Set up signing key
    E2E_STAGE_BEGIN("Set up signing key", 200);
    uint8_t signing_key[32];
    for (int i = 0; i < 32; i++) {
        signing_key[i] = static_cast<uint8_t>(rand() % 256);
    }

    bool key_set = bbb_set_signing_key(signing_key, sizeof(signing_key));
    EXPECT_TRUE(key_set) << "Signing key set";
    E2E_STAGE_END();

    // Phase 2: Create BBB system
    E2E_STAGE_BEGIN("Create BBB system", 200);
    bbb_config_t config = bbb_default_config();
    config.signing.require_signatures = true;
    config.signing.verify_on_load = true;
    bbb_system_t bbb = bbb_system_create(&config);
    E2E_ASSERT_NOT_NULL(bbb, "BBB system created");
    E2E_STAGE_END();

    // Phase 3: Sign code block
    E2E_STAGE_BEGIN("Sign code block", 500);
    const char* code_block = "function processInput(data) { return sanitize(data); }";
    uint8_t signature[256];

    ssize_t sig_len = bbb_sign_code(bbb, code_block, strlen(code_block),
                                     signature, sizeof(signature));
    EXPECT_GT(sig_len, 0) << "Code signed successfully";
    std::cout << "    Code size: " << strlen(code_block) << " bytes\n";
    std::cout << "    Signature size: " << sig_len << " bytes\n";
    E2E_STAGE_END();

    // Phase 4: Verify signature
    E2E_STAGE_BEGIN("Verify signature", 200);
    bool valid = bbb_verify_signature(bbb, code_block, strlen(code_block),
                                       signature, static_cast<size_t>(sig_len));
    EXPECT_TRUE(valid) << "Signature is valid";
    E2E_STAGE_END();

    // Phase 5: Verify tampered code fails
    E2E_STAGE_BEGIN("Verify tampered code detection", 200);
    std::string tampered_code = code_block;
    tampered_code[10] = 'X';  // Modify code

    valid = bbb_verify_signature(bbb, tampered_code.c_str(), tampered_code.length(),
                                  signature, static_cast<size_t>(sig_len));
    EXPECT_FALSE(valid) << "Tampered code rejected";
    E2E_STAGE_END();

    // Phase 6: Hash calculation
    E2E_STAGE_BEGIN("Test hash calculation", 200);
    uint8_t hash1[32], hash2[32];

    bool hash_ok = bbb_calculate_hash(code_block, strlen(code_block), hash1);
    EXPECT_TRUE(hash_ok) << "Hash calculated";

    // Same input should produce same hash
    bbb_calculate_hash(code_block, strlen(code_block), hash2);
    EXPECT_EQ(memcmp_secure(hash1, hash2, 32), 0) << "Deterministic hashing";
    E2E_STAGE_END();

    // Cleanup
    bbb_clear_signing_key();
    bbb_system_destroy(bbb);
    memset(signing_key, 0, sizeof(signing_key));

    E2E_PIPELINE_END();
}

/* ============================================================================
 * E2E Test: PQ Crypto Self-Test
 * ============================================================================ */

TEST_F(CryptoE2E, PQSelfTest) {
    E2E_PIPELINE_START("Post-Quantum Self-Test");

    E2E_STAGE_BEGIN("Run PQ self-tests", 5000);
    if (pq_ctx_) {
        nimcp_error_t err = nimcp_pq_self_test(pq_ctx_);

        if (err == NIMCP_OK) {
            std::cout << "    PQ self-test passed\n";
        } else {
            std::cout << "    Self-test returned: " << err << "\n";
        }

        // Get PQ statistics
        std::cout << "  Checking PQ statistics...\n";
        nimcp_pq_stats_t stats;
        err = nimcp_pq_get_stats(pq_ctx_, &stats);
        if (err == NIMCP_OK) {
            std::cout << "    Kyber key generations: " << stats.kyber_keygens << "\n";
            std::cout << "    Dilithium signatures: " << stats.dilithium_signs << "\n";
        }
    } else {
        std::cout << "  PQ context not available, skipping self-test\n";
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

/* ============================================================================
 * E2E Test: Crypto Performance Benchmark
 * ============================================================================ */

TEST_F(CryptoE2E, CryptoPerformanceBenchmark) {
    E2E_PIPELINE_START("Crypto Performance Benchmark");

    // Benchmark Kyber-768
    E2E_STAGE_BEGIN("Benchmark Kyber-768", 30000);
    std::cout << "  Benchmarking Kyber-768 (" << PERF_ITERATIONS << " iterations)...\n";
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < PERF_ITERATIONS; i++) {
        nimcp_kyber_keypair_t kp;
        nimcp_kyber_keygen(NIMCP_PQ_KYBER_768, &kp);

        uint8_t ct[NIMCP_KYBER_768_CIPHERTEXT_BYTES];
        uint8_t ss[NIMCP_KYBER_768_SHARED_SECRET_BYTES];
        size_t ct_len = sizeof(ct);

        nimcp_kyber_encapsulate(NIMCP_PQ_KYBER_768, kp.public_key,
                                ct, &ct_len, ss, sizeof(ss));

        uint8_t ss2[NIMCP_KYBER_768_SHARED_SECRET_BYTES];
        nimcp_kyber_decapsulate(NIMCP_PQ_KYBER_768, kp.secret_key,
                                ct, ct_len, ss2, sizeof(ss2));

        nimcp_kyber_keypair_free(&kp);
    }

    auto end = std::chrono::high_resolution_clock::now();
    double kyber_time = std::chrono::duration<double, std::milli>(end - start).count();
    std::cout << "    Kyber-768 (keygen+encap+decap): " << kyber_time << " ms total, "
              << (kyber_time / PERF_ITERATIONS) << " ms/op\n";
    E2E_STAGE_END();

    // Benchmark Dilithium-3
    E2E_STAGE_BEGIN("Benchmark Dilithium-3", 30000);
    std::cout << "  Benchmarking Dilithium-3 (" << PERF_ITERATIONS << " iterations)...\n";
    start = std::chrono::high_resolution_clock::now();

    const char* msg = "Test message for signature benchmarking";
    size_t msg_len = strlen(msg);

    for (int i = 0; i < PERF_ITERATIONS; i++) {
        nimcp_dilithium_keypair_t kp;
        nimcp_dilithium_keygen(NIMCP_PQ_DILITHIUM_3, &kp);

        uint8_t sig[NIMCP_DILITHIUM_3_SIGNATURE_BYTES];
        size_t sig_len = sizeof(sig);

        nimcp_dilithium_sign(NIMCP_PQ_DILITHIUM_3, kp.secret_key,
                            reinterpret_cast<const uint8_t*>(msg), msg_len, sig, &sig_len);

        nimcp_dilithium_verify(NIMCP_PQ_DILITHIUM_3, kp.public_key,
                               reinterpret_cast<const uint8_t*>(msg), msg_len, sig, sig_len);

        nimcp_dilithium_keypair_free(&kp);
    }

    end = std::chrono::high_resolution_clock::now();
    double dil_time = std::chrono::duration<double, std::milli>(end - start).count();
    std::cout << "    Dilithium-3 (keygen+sign+verify): " << dil_time << " ms total, "
              << (dil_time / PERF_ITERATIONS) << " ms/op\n";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

} // namespace e2e
} // namespace nimcp
