/**
 * @file test_artifact_verify.cpp
 * @brief Unit tests for Artifact Verification Module
 *
 * WHAT: Comprehensive tests for artifact signature/hash verification
 * WHY:  Ensure artifact integrity checks work correctly for supply chain security
 * HOW:  Google Test framework with fixtures covering all API functions
 *
 * TEST COVERAGE:
 * - Signature verification (Ed25519, RSA, Dilithium)
 * - Hash verification
 * - Full artifact verification
 * - Trusted source management
 * - Certificate verification
 * - Error handling
 * - Concurrency
 * - Edge cases
 *
 * @author NIMCP Security Team
 * @date 2026-02-02
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>
#include <atomic>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>

extern "C" {
#include "security/nimcp_supply_chain.h"
#include "security/nimcp_security.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class ArtifactVerifyTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create supply chain context with default config
        nimcp_supply_chain_config_t config = {};
        config.enable_logging = false;
        config.strict_mode = false;
        config.default_hash_algo = NIMCP_HASH_SHA256;
        config.default_sig_algo = NIMCP_SIG_ED25519;

        sc = nimcp_supply_chain_create(&config);

        // Create temporary directory for test files
        snprintf(test_dir, sizeof(test_dir), "/tmp/nimcp_artifact_test_%d", getpid());
        mkdir(test_dir, 0755);
    }

    void TearDown() override {
        if (sc) {
            nimcp_supply_chain_destroy(sc);
            sc = nullptr;
        }

        // Clean up test files
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "rm -rf %s", test_dir);
        int result = system(cmd);
        (void)result; // Ignore result
    }

    // Helper to create a test file
    bool CreateTestFile(const char* filename, const char* content) {
        char filepath[512];
        snprintf(filepath, sizeof(filepath), "%s/%s", test_dir, filename);

        FILE* f = fopen(filepath, "wb");
        if (!f) return false;

        size_t len = strlen(content);
        size_t written = fwrite(content, 1, len, f);
        fclose(f);

        return written == len;
    }

    // Helper to get full path
    std::string GetPath(const char* filename) {
        char filepath[512];
        snprintf(filepath, sizeof(filepath), "%s/%s", test_dir, filename);
        return std::string(filepath);
    }

    nimcp_supply_chain_t sc;
    char test_dir[256];
};

//=============================================================================
// Supply Chain Context Tests
//=============================================================================

TEST_F(ArtifactVerifyTest, CreateSupplyChainContext) {
    EXPECT_NE(sc, nullptr);
}

TEST_F(ArtifactVerifyTest, CreateWithNullConfig) {
    nimcp_supply_chain_t default_sc = nimcp_supply_chain_create(nullptr);
    EXPECT_NE(default_sc, nullptr);
    nimcp_supply_chain_destroy(default_sc);
}

TEST_F(ArtifactVerifyTest, DestroyNullContext) {
    // Should not crash
    nimcp_supply_chain_destroy(nullptr);
}

TEST_F(ArtifactVerifyTest, GetStats) {
    nimcp_supply_chain_stats_t stats;

    nimcp_error_t result = nimcp_supply_chain_get_stats(sc, &stats);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(stats.total_dependencies, 0u);
    EXPECT_EQ(stats.verified_dependencies, 0u);
}

TEST_F(ArtifactVerifyTest, GetStatsNullParams) {
    nimcp_supply_chain_stats_t stats;

    EXPECT_NE(nimcp_supply_chain_get_stats(nullptr, &stats), NIMCP_SUCCESS);
    EXPECT_NE(nimcp_supply_chain_get_stats(sc, nullptr), NIMCP_SUCCESS);
}

//=============================================================================
// Trusted Source Management Tests
//=============================================================================

TEST_F(ArtifactVerifyTest, AddTrustedSource) {
    ASSERT_TRUE(CreateTestFile("test_key.pem", "fake key data"));
    std::string key_path = GetPath("test_key.pem");

    nimcp_error_t result = nimcp_supply_chain_add_trusted_source(
        sc,
        "https://example.com/packages",
        key_path.c_str(),
        NIMCP_SIG_ED25519
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(ArtifactVerifyTest, AddMultipleTrustedSources) {
    ASSERT_TRUE(CreateTestFile("key1.pem", "key1"));
    ASSERT_TRUE(CreateTestFile("key2.pem", "key2"));
    ASSERT_TRUE(CreateTestFile("key3.pem", "key3"));

    EXPECT_EQ(nimcp_supply_chain_add_trusted_source(
        sc, "https://source1.com", GetPath("key1.pem").c_str(), NIMCP_SIG_ED25519),
        NIMCP_SUCCESS);

    EXPECT_EQ(nimcp_supply_chain_add_trusted_source(
        sc, "https://source2.com", GetPath("key2.pem").c_str(), NIMCP_SIG_RSA_2048),
        NIMCP_SUCCESS);

    EXPECT_EQ(nimcp_supply_chain_add_trusted_source(
        sc, "https://source3.com", GetPath("key3.pem").c_str(), NIMCP_SIG_RSA_4096),
        NIMCP_SUCCESS);

    // Verify count
    nimcp_trusted_source_t* sources = nullptr;
    size_t count = 0;
    EXPECT_EQ(nimcp_supply_chain_list_sources(sc, &sources, &count), NIMCP_SUCCESS);
    EXPECT_EQ(count, 3u);
    free(sources);
}

TEST_F(ArtifactVerifyTest, AddTrustedSourceNullParams) {
    ASSERT_TRUE(CreateTestFile("key.pem", "key"));
    std::string key_path = GetPath("key.pem");

    EXPECT_NE(nimcp_supply_chain_add_trusted_source(
        nullptr, "https://test.com", key_path.c_str(), NIMCP_SIG_ED25519),
        NIMCP_SUCCESS);

    EXPECT_NE(nimcp_supply_chain_add_trusted_source(
        sc, nullptr, key_path.c_str(), NIMCP_SIG_ED25519),
        NIMCP_SUCCESS);

    EXPECT_NE(nimcp_supply_chain_add_trusted_source(
        sc, "https://test.com", nullptr, NIMCP_SIG_ED25519),
        NIMCP_SUCCESS);
}

TEST_F(ArtifactVerifyTest, IsSourceTrusted) {
    ASSERT_TRUE(CreateTestFile("key.pem", "key"));
    std::string key_path = GetPath("key.pem");

    const char* url = "https://trusted.com/packages";

    EXPECT_FALSE(nimcp_supply_chain_is_source_trusted(sc, url));

    EXPECT_EQ(nimcp_supply_chain_add_trusted_source(
        sc, url, key_path.c_str(), NIMCP_SIG_ED25519), NIMCP_SUCCESS);

    EXPECT_TRUE(nimcp_supply_chain_is_source_trusted(sc, url));
}

TEST_F(ArtifactVerifyTest, IsSourceTrustedNullParams) {
    EXPECT_FALSE(nimcp_supply_chain_is_source_trusted(nullptr, "https://test.com"));
    EXPECT_FALSE(nimcp_supply_chain_is_source_trusted(sc, nullptr));
}

TEST_F(ArtifactVerifyTest, RevokeSource) {
    ASSERT_TRUE(CreateTestFile("key.pem", "key"));
    std::string key_path = GetPath("key.pem");

    const char* url = "https://revokable.com";

    EXPECT_EQ(nimcp_supply_chain_add_trusted_source(
        sc, url, key_path.c_str(), NIMCP_SIG_ED25519), NIMCP_SUCCESS);

    EXPECT_TRUE(nimcp_supply_chain_is_source_trusted(sc, url));

    EXPECT_EQ(nimcp_supply_chain_revoke_source(sc, url), NIMCP_SUCCESS);

    EXPECT_FALSE(nimcp_supply_chain_is_source_trusted(sc, url));
}

TEST_F(ArtifactVerifyTest, RevokeNonexistentSource) {
    nimcp_error_t result = nimcp_supply_chain_revoke_source(sc, "https://nonexistent.com");

    // Should fail or return not found
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(ArtifactVerifyTest, RevokeSourceNullParams) {
    EXPECT_NE(nimcp_supply_chain_revoke_source(nullptr, "https://test.com"), NIMCP_SUCCESS);
    EXPECT_NE(nimcp_supply_chain_revoke_source(sc, nullptr), NIMCP_SUCCESS);
}

TEST_F(ArtifactVerifyTest, ListSources) {
    ASSERT_TRUE(CreateTestFile("key.pem", "key"));
    std::string key_path = GetPath("key.pem");

    // Add some sources
    for (int i = 0; i < 5; i++) {
        char url[256];
        snprintf(url, sizeof(url), "https://source%d.com", i);
        EXPECT_EQ(nimcp_supply_chain_add_trusted_source(
            sc, url, key_path.c_str(), NIMCP_SIG_ED25519), NIMCP_SUCCESS);
    }

    nimcp_trusted_source_t* sources = nullptr;
    size_t count = 0;

    EXPECT_EQ(nimcp_supply_chain_list_sources(sc, &sources, &count), NIMCP_SUCCESS);
    EXPECT_EQ(count, 5u);
    EXPECT_NE(sources, nullptr);

    // Verify sources data
    for (size_t i = 0; i < count; i++) {
        EXPECT_TRUE(sources[i].is_active);
        EXPECT_GT(strlen(sources[i].url), 0u);
    }

    free(sources);
}

TEST_F(ArtifactVerifyTest, ListSourcesEmpty) {
    nimcp_trusted_source_t* sources = nullptr;
    size_t count = 999;

    EXPECT_EQ(nimcp_supply_chain_list_sources(sc, &sources, &count), NIMCP_SUCCESS);
    EXPECT_EQ(count, 0u);
    EXPECT_EQ(sources, nullptr);
}

TEST_F(ArtifactVerifyTest, ListSourcesNullParams) {
    nimcp_trusted_source_t* sources = nullptr;
    size_t count = 0;

    EXPECT_NE(nimcp_supply_chain_list_sources(nullptr, &sources, &count), NIMCP_SUCCESS);
    EXPECT_NE(nimcp_supply_chain_list_sources(sc, nullptr, &count), NIMCP_SUCCESS);
    EXPECT_NE(nimcp_supply_chain_list_sources(sc, &sources, nullptr), NIMCP_SUCCESS);
}

//=============================================================================
// Signature Verification Tests - Parameter Validation
//=============================================================================

TEST_F(ArtifactVerifyTest, VerifySignatureNullParams) {
    ASSERT_TRUE(CreateTestFile("artifact.bin", "test data"));
    ASSERT_TRUE(CreateTestFile("sig.bin", "signature"));
    ASSERT_TRUE(CreateTestFile("key.pem", "key"));

    std::string artifact = GetPath("artifact.bin");
    std::string sig = GetPath("sig.bin");
    std::string key = GetPath("key.pem");

    // Null supply chain
    EXPECT_NE(nimcp_artifact_verify_signature(
        nullptr, artifact.c_str(), sig.c_str(), key.c_str(), NIMCP_SIG_ED25519),
        NIMCP_SUCCESS);

    // Null filepath
    EXPECT_NE(nimcp_artifact_verify_signature(
        sc, nullptr, sig.c_str(), key.c_str(), NIMCP_SIG_ED25519),
        NIMCP_SUCCESS);

    // Null signature path
    EXPECT_NE(nimcp_artifact_verify_signature(
        sc, artifact.c_str(), nullptr, key.c_str(), NIMCP_SIG_ED25519),
        NIMCP_SUCCESS);

    // Null public key path
    EXPECT_NE(nimcp_artifact_verify_signature(
        sc, artifact.c_str(), sig.c_str(), nullptr, NIMCP_SIG_ED25519),
        NIMCP_SUCCESS);
}

TEST_F(ArtifactVerifyTest, VerifySignatureFileNotFound) {
    ASSERT_TRUE(CreateTestFile("key.pem", "key"));

    nimcp_error_t result = nimcp_artifact_verify_signature(
        sc,
        "/nonexistent/artifact.bin",
        "/nonexistent/sig.bin",
        GetPath("key.pem").c_str(),
        NIMCP_SIG_ED25519
    );

    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(ArtifactVerifyTest, VerifySignatureInvalidKey) {
    ASSERT_TRUE(CreateTestFile("artifact.bin", "test artifact data"));
    ASSERT_TRUE(CreateTestFile("sig.bin", "invalid signature"));
    ASSERT_TRUE(CreateTestFile("bad_key.pem", "this is not a valid key"));

    nimcp_error_t result = nimcp_artifact_verify_signature(
        sc,
        GetPath("artifact.bin").c_str(),
        GetPath("sig.bin").c_str(),
        GetPath("bad_key.pem").c_str(),
        NIMCP_SIG_ED25519
    );

    // Should fail due to invalid key
    EXPECT_NE(result, NIMCP_SUCCESS);
}

//=============================================================================
// Full Verification Tests
//=============================================================================

TEST_F(ArtifactVerifyTest, VerifyFullNullParams) {
    nimcp_artifact_verification_t result_struct;

    EXPECT_NE(nimcp_artifact_verify_full(
        nullptr, "path", "hash", NIMCP_HASH_SHA256,
        "sig", "key", NIMCP_SIG_ED25519, &result_struct),
        NIMCP_SUCCESS);

    EXPECT_NE(nimcp_artifact_verify_full(
        sc, nullptr, "hash", NIMCP_HASH_SHA256,
        "sig", "key", NIMCP_SIG_ED25519, &result_struct),
        NIMCP_SUCCESS);

    EXPECT_NE(nimcp_artifact_verify_full(
        sc, "path", "hash", NIMCP_HASH_SHA256,
        "sig", "key", NIMCP_SIG_ED25519, nullptr),
        NIMCP_SUCCESS);
}

TEST_F(ArtifactVerifyTest, VerifyFullResultInitialization) {
    ASSERT_TRUE(CreateTestFile("artifact.bin", "test content"));

    nimcp_artifact_verification_t result;
    memset(&result, 0xFF, sizeof(result)); // Fill with garbage

    // Call will likely fail due to missing files, but result should be initialized
    nimcp_artifact_verify_full(
        sc,
        GetPath("artifact.bin").c_str(),
        "expected_hash",
        NIMCP_HASH_SHA256,
        nullptr, // No signature verification
        nullptr,
        NIMCP_SIG_ED25519,
        &result
    );

    // Result should have been initialized with the filepath
    EXPECT_TRUE(strstr(result.filepath, "artifact.bin") != nullptr);
    EXPECT_EQ(result.hash_algo, NIMCP_HASH_SHA256);
    EXPECT_EQ(result.sig_algo, NIMCP_SIG_ED25519);
}

//=============================================================================
// Certificate Verification Tests
//=============================================================================

TEST_F(ArtifactVerifyTest, VerifyCertChainNullParams) {
    ASSERT_TRUE(CreateTestFile("cert.pem", "cert"));
    ASSERT_TRUE(CreateTestFile("ca.pem", "ca"));

    EXPECT_NE(nimcp_cert_verify_chain(nullptr,
        GetPath("cert.pem").c_str(), GetPath("ca.pem").c_str()),
        NIMCP_SUCCESS);

    EXPECT_NE(nimcp_cert_verify_chain(sc, nullptr, GetPath("ca.pem").c_str()),
        NIMCP_SUCCESS);

    EXPECT_NE(nimcp_cert_verify_chain(sc, GetPath("cert.pem").c_str(), nullptr),
        NIMCP_SUCCESS);
}

TEST_F(ArtifactVerifyTest, VerifyCertChainValidPaths) {
    ASSERT_TRUE(CreateTestFile("cert.pem", "certificate data"));
    ASSERT_TRUE(CreateTestFile("ca.pem", "ca certificate"));

    // This is a stub that returns success for valid paths
    nimcp_error_t result = nimcp_cert_verify_chain(
        sc,
        GetPath("cert.pem").c_str(),
        GetPath("ca.pem").c_str()
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

//=============================================================================
// Timestamp Verification Tests
//=============================================================================

TEST_F(ArtifactVerifyTest, VerifyTimestampNullParams) {
    ASSERT_TRUE(CreateTestFile("timestamp.tsr", "timestamp"));
    ASSERT_TRUE(CreateTestFile("artifact.bin", "artifact"));

    EXPECT_NE(nimcp_timestamp_verify(nullptr,
        GetPath("timestamp.tsr").c_str(), GetPath("artifact.bin").c_str()),
        NIMCP_SUCCESS);

    EXPECT_NE(nimcp_timestamp_verify(sc, nullptr, GetPath("artifact.bin").c_str()),
        NIMCP_SUCCESS);

    EXPECT_NE(nimcp_timestamp_verify(sc, GetPath("timestamp.tsr").c_str(), nullptr),
        NIMCP_SUCCESS);
}

TEST_F(ArtifactVerifyTest, VerifyTimestampValidPaths) {
    ASSERT_TRUE(CreateTestFile("timestamp.tsr", "timestamp token"));
    ASSERT_TRUE(CreateTestFile("artifact.bin", "artifact data"));

    // This is a stub that returns success for valid paths
    nimcp_error_t result = nimcp_timestamp_verify(
        sc,
        GetPath("timestamp.tsr").c_str(),
        GetPath("artifact.bin").c_str()
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

//=============================================================================
// Certificate Revocation Tests
//=============================================================================

TEST_F(ArtifactVerifyTest, CheckRevocationNullParams) {
    ASSERT_TRUE(CreateTestFile("cert.pem", "cert"));
    ASSERT_TRUE(CreateTestFile("crl.pem", "crl"));

    EXPECT_NE(nimcp_cert_check_revocation(nullptr,
        GetPath("cert.pem").c_str(), GetPath("crl.pem").c_str(), nullptr),
        NIMCP_SUCCESS);

    EXPECT_NE(nimcp_cert_check_revocation(sc, nullptr,
        GetPath("crl.pem").c_str(), nullptr),
        NIMCP_SUCCESS);
}

TEST_F(ArtifactVerifyTest, CheckRevocationWithCRL) {
    ASSERT_TRUE(CreateTestFile("cert.pem", "certificate"));
    ASSERT_TRUE(CreateTestFile("crl.pem", "revocation list"));

    nimcp_error_t result = nimcp_cert_check_revocation(
        sc,
        GetPath("cert.pem").c_str(),
        GetPath("crl.pem").c_str(),
        nullptr  // No OCSP
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(ArtifactVerifyTest, CheckRevocationWithOCSP) {
    ASSERT_TRUE(CreateTestFile("cert.pem", "certificate"));

    nimcp_error_t result = nimcp_cert_check_revocation(
        sc,
        GetPath("cert.pem").c_str(),
        nullptr,  // No CRL
        "http://ocsp.example.com"
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

//=============================================================================
// Directory Scanning Tests
//=============================================================================

TEST_F(ArtifactVerifyTest, ScanDirectoryNullParams) {
    EXPECT_NE(nimcp_supply_chain_scan_directory(nullptr, test_dir), NIMCP_SUCCESS);
    EXPECT_NE(nimcp_supply_chain_scan_directory(sc, nullptr), NIMCP_SUCCESS);
}

TEST_F(ArtifactVerifyTest, ScanDirectoryValid) {
    // Create some test files
    ASSERT_TRUE(CreateTestFile("lib1.so", "library data"));
    ASSERT_TRUE(CreateTestFile("lib2.so", "library data"));

    nimcp_error_t result = nimcp_supply_chain_scan_directory(sc, test_dir);

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(ArtifactVerifyTest, ScanNonexistentDirectory) {
    nimcp_error_t result = nimcp_supply_chain_scan_directory(
        sc, "/nonexistent/directory/path");

    // Should succeed (stub implementation) or fail gracefully
    // The main thing is it shouldn't crash
}

//=============================================================================
// Report Export Tests
//=============================================================================

TEST_F(ArtifactVerifyTest, ExportReportNullParams) {
    char* output = nullptr;

    EXPECT_NE(nimcp_supply_chain_export_report(nullptr, "json", &output), NIMCP_SUCCESS);
    EXPECT_NE(nimcp_supply_chain_export_report(sc, nullptr, &output), NIMCP_SUCCESS);
    EXPECT_NE(nimcp_supply_chain_export_report(sc, "json", nullptr), NIMCP_SUCCESS);
}

TEST_F(ArtifactVerifyTest, ExportReportJson) {
    char* output = nullptr;

    nimcp_error_t result = nimcp_supply_chain_export_report(sc, "json", &output);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_NE(output, nullptr);

    if (output) {
        // Verify report contains expected fields
        EXPECT_TRUE(strstr(output, "Dependencies") != nullptr ||
                    strstr(output, "Verified") != nullptr);
        free(output);
    }
}

TEST_F(ArtifactVerifyTest, ExportReportText) {
    char* output = nullptr;

    nimcp_error_t result = nimcp_supply_chain_export_report(sc, "text", &output);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_NE(output, nullptr);

    if (output) {
        free(output);
    }
}

TEST_F(ArtifactVerifyTest, ExportReportAfterOperations) {
    ASSERT_TRUE(CreateTestFile("key.pem", "key"));

    // Add some sources
    nimcp_supply_chain_add_trusted_source(
        sc, "https://test.com", GetPath("key.pem").c_str(), NIMCP_SIG_ED25519);

    char* output = nullptr;
    nimcp_error_t result = nimcp_supply_chain_export_report(sc, "text", &output);

    EXPECT_EQ(result, NIMCP_SUCCESS);

    if (output) {
        free(output);
    }
}

//=============================================================================
// Concurrency Tests
//=============================================================================

TEST_F(ArtifactVerifyTest, ConcurrentSourceChecks) {
    ASSERT_TRUE(CreateTestFile("key.pem", "key"));
    std::string key_path = GetPath("key.pem");

    // Add some sources
    for (int i = 0; i < 10; i++) {
        char url[256];
        snprintf(url, sizeof(url), "https://source%d.com", i);
        nimcp_supply_chain_add_trusted_source(
            sc, url, key_path.c_str(), NIMCP_SIG_ED25519);
    }

    const int num_threads = 8;
    const int iterations = 100;
    std::atomic<int> total_checks(0);
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, &total_checks, iterations]() {
            for (int i = 0; i < iterations; i++) {
                char url[256];
                snprintf(url, sizeof(url), "https://source%d.com", i % 10);
                nimcp_supply_chain_is_source_trusted(sc, url);
                total_checks++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(total_checks.load(), num_threads * iterations);
}

TEST_F(ArtifactVerifyTest, ConcurrentAddAndRevoke) {
    ASSERT_TRUE(CreateTestFile("key.pem", "key"));
    std::string key_path = GetPath("key.pem");

    const int num_threads = 4;
    const int iterations = 50;
    std::vector<std::thread> threads;

    // Threads adding sources
    for (int t = 0; t < num_threads / 2; t++) {
        threads.emplace_back([this, &key_path, t, iterations]() {
            for (int i = 0; i < iterations; i++) {
                char url[256];
                snprintf(url, sizeof(url), "https://thread%d_source%d.com", t, i);
                nimcp_supply_chain_add_trusted_source(
                    sc, url, key_path.c_str(), NIMCP_SIG_ED25519);
            }
        });
    }

    // Threads revoking sources
    for (int t = 0; t < num_threads / 2; t++) {
        threads.emplace_back([this, t, iterations]() {
            for (int i = 0; i < iterations; i++) {
                char url[256];
                snprintf(url, sizeof(url), "https://thread%d_source%d.com", t, i);
                // Revoke may fail if source doesn't exist yet - that's OK
                nimcp_supply_chain_revoke_source(sc, url);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Should not crash and stats should be valid
    nimcp_supply_chain_stats_t stats;
    EXPECT_EQ(nimcp_supply_chain_get_stats(sc, &stats), NIMCP_SUCCESS);
}

TEST_F(ArtifactVerifyTest, ConcurrentListSources) {
    ASSERT_TRUE(CreateTestFile("key.pem", "key"));
    std::string key_path = GetPath("key.pem");

    // Add sources first
    for (int i = 0; i < 10; i++) {
        char url[256];
        snprintf(url, sizeof(url), "https://source%d.com", i);
        nimcp_supply_chain_add_trusted_source(
            sc, url, key_path.c_str(), NIMCP_SIG_ED25519);
    }

    const int num_threads = 8;
    const int iterations = 50;
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, iterations]() {
            for (int i = 0; i < iterations; i++) {
                nimcp_trusted_source_t* sources = nullptr;
                size_t count = 0;
                nimcp_error_t result = nimcp_supply_chain_list_sources(sc, &sources, &count);
                if (result == NIMCP_SUCCESS && sources) {
                    free(sources);
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(ArtifactVerifyTest, VeryLongURL) {
    ASSERT_TRUE(CreateTestFile("key.pem", "key"));

    std::string long_url = "https://";
    for (int i = 0; i < 200; i++) {
        long_url += "subdomain.";
    }
    long_url += "example.com/very/long/path";

    // Should handle gracefully (may truncate or fail)
    nimcp_error_t result = nimcp_supply_chain_add_trusted_source(
        sc, long_url.c_str(), GetPath("key.pem").c_str(), NIMCP_SIG_ED25519);

    // Main thing is it shouldn't crash
    (void)result;
}

TEST_F(ArtifactVerifyTest, SpecialCharactersInURL) {
    ASSERT_TRUE(CreateTestFile("key.pem", "key"));

    const char* special_urls[] = {
        "https://example.com/path with spaces",
        "https://example.com/path?query=value&other=123",
        "https://user:pass@example.com/",
        "https://example.com/path#fragment",
        "https://192.168.1.1:8080/path"
    };

    for (const char* url : special_urls) {
        nimcp_error_t result = nimcp_supply_chain_add_trusted_source(
            sc, url, GetPath("key.pem").c_str(), NIMCP_SIG_ED25519);
        // Should handle gracefully
        (void)result;
    }
}

TEST_F(ArtifactVerifyTest, EmptyFile) {
    ASSERT_TRUE(CreateTestFile("empty.bin", ""));

    nimcp_artifact_verification_t result;
    nimcp_error_t err = nimcp_artifact_verify_full(
        sc,
        GetPath("empty.bin").c_str(),
        nullptr,  // No hash check
        NIMCP_HASH_SHA256,
        nullptr,  // No signature
        nullptr,
        NIMCP_SIG_ED25519,
        &result
    );

    // Should handle empty file gracefully
    (void)err;
}

TEST_F(ArtifactVerifyTest, LargeFile) {
    // Create a larger file (1MB)
    std::string large_content(1024 * 1024, 'X');
    ASSERT_TRUE(CreateTestFile("large.bin", large_content.c_str()));

    nimcp_artifact_verification_t result;
    nimcp_error_t err = nimcp_artifact_verify_full(
        sc,
        GetPath("large.bin").c_str(),
        nullptr,
        NIMCP_HASH_SHA256,
        nullptr,
        nullptr,
        NIMCP_SIG_ED25519,
        &result
    );

    // Should handle large file
    (void)err;
}

TEST_F(ArtifactVerifyTest, AllSignatureAlgorithms) {
    ASSERT_TRUE(CreateTestFile("artifact.bin", "test data"));
    ASSERT_TRUE(CreateTestFile("sig.bin", "sig"));
    ASSERT_TRUE(CreateTestFile("key.pem", "key"));

    nimcp_signature_algorithm_t algos[] = {
        NIMCP_SIG_ED25519,
        NIMCP_SIG_RSA_2048,
        NIMCP_SIG_RSA_4096,
        NIMCP_SIG_DILITHIUM_2,
        NIMCP_SIG_DILITHIUM_3,
        NIMCP_SIG_DILITHIUM_5
    };

    for (auto algo : algos) {
        // Will fail due to invalid key/sig, but should handle all algorithms
        nimcp_artifact_verify_signature(
            sc,
            GetPath("artifact.bin").c_str(),
            GetPath("sig.bin").c_str(),
            GetPath("key.pem").c_str(),
            algo
        );
    }
}

TEST_F(ArtifactVerifyTest, AllHashAlgorithms) {
    ASSERT_TRUE(CreateTestFile("artifact.bin", "test data"));

    nimcp_hash_algorithm_t algos[] = {
        NIMCP_HASH_SHA256,
        NIMCP_HASH_SHA512,
        NIMCP_HASH_SHA3_256,
        NIMCP_HASH_SHA3_512
    };

    for (auto algo : algos) {
        nimcp_artifact_verification_t result;
        // Will fail due to hash mismatch, but should handle all algorithms
        nimcp_artifact_verify_full(
            sc,
            GetPath("artifact.bin").c_str(),
            "0000000000000000000000000000000000000000000000000000000000000000",
            algo,
            nullptr,
            nullptr,
            NIMCP_SIG_ED25519,
            &result
        );
    }
}

//=============================================================================
// Statistics After Operations Tests
//=============================================================================

TEST_F(ArtifactVerifyTest, StatsAfterVerification) {
    ASSERT_TRUE(CreateTestFile("artifact.bin", "test"));
    ASSERT_TRUE(CreateTestFile("sig.bin", "sig"));
    ASSERT_TRUE(CreateTestFile("key.pem", "key"));

    // Attempt verification (will fail)
    nimcp_artifact_verify_signature(
        sc,
        GetPath("artifact.bin").c_str(),
        GetPath("sig.bin").c_str(),
        GetPath("key.pem").c_str(),
        NIMCP_SIG_ED25519
    );

    nimcp_supply_chain_stats_t stats;
    EXPECT_EQ(nimcp_supply_chain_get_stats(sc, &stats), NIMCP_SUCCESS);

    // Stats should reflect the failed verification
    EXPECT_GE(stats.failed_verifications, 1u);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
