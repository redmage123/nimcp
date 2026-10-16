/**
 * @file test_supply_chain.cpp
 * @brief Unit Tests for Supply Chain Security
 *
 * Tests SBOM generation, artifact verification, trusted sources
 */

#include <gtest/gtest.h>
// Headers have their own extern "C" guards
    #include "security/nimcp_supply_chain.h"
    #include "utils/error/nimcp_error_codes.h"
#include <cstring>
#include <fstream>
#include <string>

class SupplyChainTest : public ::testing::Test {
protected:
    nimcp_supply_chain_t sc;
    std::string test_file_path;
    std::string test_sig_path;
    std::string test_key_path;

    void SetUp() override {
        nimcp_supply_chain_config_t config;
        memset(&config, 0, sizeof(config));
        config.enable_logging = true;
        config.strict_mode = false;
        config.default_hash_algo = NIMCP_HASH_SHA256;
        config.bio_ctx = nullptr;

        sc = nimcp_supply_chain_create(&config);
        ASSERT_NE(sc, nullptr);

        // Create test file
        test_file_path = "/tmp/nimcp_test_file.txt";
        std::ofstream test_file(test_file_path);
        test_file << "Test content for supply chain verification\n";
        test_file.close();
    }

    void TearDown() override {
        if (sc) {
            nimcp_supply_chain_destroy(sc);
        }
        // Clean up test files
        std::remove(test_file_path.c_str());
    }
};

/* ========================================================================
 * Context Tests
 * ======================================================================== */

TEST_F(SupplyChainTest, ContextCreateDestroy) {
    nimcp_supply_chain_t test_sc = nimcp_supply_chain_create(nullptr);
    EXPECT_NE(test_sc, nullptr);
    nimcp_supply_chain_destroy(test_sc);
}

TEST_F(SupplyChainTest, ContextGetStats) {
    nimcp_supply_chain_stats_t stats;
    EXPECT_EQ(nimcp_supply_chain_get_stats(sc, &stats), NIMCP_SUCCESS);

    // Initial stats should be zero
    EXPECT_EQ(stats.total_dependencies, 0);
    EXPECT_EQ(stats.verified_dependencies, 0);
    EXPECT_EQ(stats.failed_verifications, 0);
}

/* ========================================================================
 * Hash Verification Tests
 * ======================================================================== */

TEST_F(SupplyChainTest, ComputeHashSHA256) {
    char hash[129];
    EXPECT_EQ(nimcp_artifact_compute_hash(sc, test_file_path.c_str(),
                                           NIMCP_HASH_SHA256, hash),
              NIMCP_SUCCESS);

    // Hash should be 64 hex characters (32 bytes)
    EXPECT_EQ(strlen(hash), 64);

    // Should be all hex digits
    for (size_t i = 0; i < 64; i++) {
        EXPECT_TRUE((hash[i] >= '0' && hash[i] <= '9') ||
                   (hash[i] >= 'a' && hash[i] <= 'f'));
    }
}

TEST_F(SupplyChainTest, ComputeHashSHA512) {
    char hash[129];
    EXPECT_EQ(nimcp_artifact_compute_hash(sc, test_file_path.c_str(),
                                           NIMCP_HASH_SHA512, hash),
              NIMCP_SUCCESS);

    // Hash should be 128 hex characters (64 bytes)
    EXPECT_EQ(strlen(hash), 128);
}

TEST_F(SupplyChainTest, VerifyHashCorrect) {
    // Compute expected hash
    char expected_hash[129];
    ASSERT_EQ(nimcp_artifact_compute_hash(sc, test_file_path.c_str(),
                                           NIMCP_HASH_SHA256, expected_hash),
              NIMCP_SUCCESS);

    // Verify should succeed
    EXPECT_EQ(nimcp_artifact_verify_hash(sc, test_file_path.c_str(),
                                          expected_hash, NIMCP_HASH_SHA256),
              NIMCP_SUCCESS);

    // Check stats updated
    nimcp_supply_chain_stats_t stats;
    nimcp_supply_chain_get_stats(sc, &stats);
    EXPECT_GT(stats.verified_dependencies, 0);
}

TEST_F(SupplyChainTest, VerifyHashIncorrect) {
    const char* wrong_hash = "0000000000000000000000000000000000000000000000000000000000000000";

    EXPECT_EQ(nimcp_artifact_verify_hash(sc, test_file_path.c_str(),
                                          wrong_hash, NIMCP_HASH_SHA256),
              NIMCP_ERROR_VERIFICATION_FAILED);

    // Check stats updated
    nimcp_supply_chain_stats_t stats;
    nimcp_supply_chain_get_stats(sc, &stats);
    EXPECT_GT(stats.failed_verifications, 0);
    EXPECT_GT(stats.integrity_violations, 0);
}

TEST_F(SupplyChainTest, VerifyHashNonexistentFile) {
    char hash[129] = "1234567890abcdef";

    EXPECT_NE(nimcp_artifact_verify_hash(sc, "/nonexistent/file.txt",
                                          hash, NIMCP_HASH_SHA256),
              NIMCP_SUCCESS);
}

/* ========================================================================
 * SBOM Tests
 * ======================================================================== */

TEST_F(SupplyChainTest, SBOMAddDependency) {
    nimcp_dependency_t dep;
    memset(&dep, 0, sizeof(dep));
    strncpy(dep.name, "test-library", sizeof(dep.name) - 1);
    strncpy(dep.version, "1.0.0", sizeof(dep.version) - 1);
    strncpy(dep.license, "MIT", sizeof(dep.license) - 1);
    strncpy(dep.supplier, "Test Supplier", sizeof(dep.supplier) - 1);
    dep.is_critical = true;
    dep.is_direct = true;

    EXPECT_EQ(nimcp_sbom_add_dependency(sc, &dep), NIMCP_SUCCESS);

    // Check stats
    nimcp_supply_chain_stats_t stats;
    nimcp_supply_chain_get_stats(sc, &stats);
    EXPECT_EQ(stats.total_dependencies, 1);
}

TEST_F(SupplyChainTest, SBOMGetDependencies) {
    // Add multiple dependencies
    for (int i = 0; i < 5; i++) {
        nimcp_dependency_t dep;
        memset(&dep, 0, sizeof(dep));
        snprintf(dep.name, sizeof(dep.name), "library-%d", i);
        snprintf(dep.version, sizeof(dep.version), "1.%d.0", i);
        ASSERT_EQ(nimcp_sbom_add_dependency(sc, &dep), NIMCP_SUCCESS);
    }

    // Get dependencies
    nimcp_dependency_t* deps = nullptr;
    size_t count = 0;
    EXPECT_EQ(nimcp_sbom_get_dependencies(sc, &deps, &count), NIMCP_SUCCESS);
    EXPECT_EQ(count, 5);
    EXPECT_NE(deps, nullptr);

    free(deps);
}

TEST_F(SupplyChainTest, SBOMQueryDependency) {
    nimcp_dependency_t dep;
    memset(&dep, 0, sizeof(dep));
    strncpy(dep.name, "query-test", sizeof(dep.name) - 1);
    strncpy(dep.version, "2.0.0", sizeof(dep.version) - 1);

    ASSERT_EQ(nimcp_sbom_add_dependency(sc, &dep), NIMCP_SUCCESS);

    // Query should find it
    nimcp_dependency_t found;
    EXPECT_EQ(nimcp_sbom_query_dependency(sc, "query-test", &found), NIMCP_SUCCESS);
    EXPECT_STREQ(found.name, "query-test");
    EXPECT_STREQ(found.version, "2.0.0");

    // Query for non-existent should fail
    EXPECT_EQ(nimcp_sbom_query_dependency(sc, "nonexistent", &found),
              NIMCP_ERROR_NOT_FOUND);
}

TEST_F(SupplyChainTest, SBOMRemoveDependency) {
    nimcp_dependency_t dep;
    memset(&dep, 0, sizeof(dep));
    strncpy(dep.name, "remove-test", sizeof(dep.name) - 1);

    ASSERT_EQ(nimcp_sbom_add_dependency(sc, &dep), NIMCP_SUCCESS);

    // Remove should succeed
    EXPECT_EQ(nimcp_sbom_remove_dependency(sc, "remove-test"), NIMCP_SUCCESS);

    // Query should now fail
    nimcp_dependency_t found;
    EXPECT_EQ(nimcp_sbom_query_dependency(sc, "remove-test", &found),
              NIMCP_ERROR_NOT_FOUND);

    // Remove non-existent should fail
    EXPECT_EQ(nimcp_sbom_remove_dependency(sc, "nonexistent"),
              NIMCP_ERROR_NOT_FOUND);
}

TEST_F(SupplyChainTest, SBOMGenerateSPDX) {
    // Add some dependencies
    nimcp_dependency_t dep;
    memset(&dep, 0, sizeof(dep));
    strncpy(dep.name, "openssl", sizeof(dep.name) - 1);
    strncpy(dep.version, "3.0.0", sizeof(dep.version) - 1);
    strncpy(dep.license, "Apache-2.0", sizeof(dep.license) - 1);
    ASSERT_EQ(nimcp_sbom_add_dependency(sc, &dep), NIMCP_SUCCESS);

    // Generate SPDX
    char* sbom = nullptr;
    EXPECT_EQ(nimcp_sbom_generate(sc, NIMCP_SBOM_FORMAT_SPDX, &sbom), NIMCP_SUCCESS);
    EXPECT_NE(sbom, nullptr);

    // Check SPDX header
    EXPECT_NE(strstr(sbom, "SPDXVersion"), nullptr);
    EXPECT_NE(strstr(sbom, "openssl"), nullptr);

    free(sbom);
}

TEST_F(SupplyChainTest, SBOMGenerateCycloneDX) {
    // Add dependency
    nimcp_dependency_t dep;
    memset(&dep, 0, sizeof(dep));
    strncpy(dep.name, "zlib", sizeof(dep.name) - 1);
    strncpy(dep.version, "1.2.11", sizeof(dep.version) - 1);
    ASSERT_EQ(nimcp_sbom_add_dependency(sc, &dep), NIMCP_SUCCESS);

    // Generate CycloneDX
    char* sbom = nullptr;
    EXPECT_EQ(nimcp_sbom_generate(sc, NIMCP_SBOM_FORMAT_CYCLONEDX, &sbom), NIMCP_SUCCESS);
    EXPECT_NE(sbom, nullptr);

    // Check CycloneDX format
    EXPECT_NE(strstr(sbom, "bomFormat"), nullptr);
    EXPECT_NE(strstr(sbom, "CycloneDX"), nullptr);
    EXPECT_NE(strstr(sbom, "zlib"), nullptr);

    free(sbom);
}

TEST_F(SupplyChainTest, SBOMSaveLoad) {
    // Add dependency
    nimcp_dependency_t dep;
    memset(&dep, 0, sizeof(dep));
    strncpy(dep.name, "test-dep", sizeof(dep.name) - 1);
    ASSERT_EQ(nimcp_sbom_add_dependency(sc, &dep), NIMCP_SUCCESS);

    // Save SBOM
    std::string sbom_path = "/tmp/nimcp_test_sbom.spdx";
    EXPECT_EQ(nimcp_sbom_save(sc, sbom_path.c_str(), NIMCP_SBOM_FORMAT_SPDX),
              NIMCP_SUCCESS);

    // Load SBOM
    EXPECT_EQ(nimcp_sbom_load(sc, sbom_path.c_str(), NIMCP_SBOM_FORMAT_SPDX),
              NIMCP_SUCCESS);

    // Check stats
    nimcp_supply_chain_stats_t stats;
    nimcp_supply_chain_get_stats(sc, &stats);
    EXPECT_GT(stats.sbom_loads, 0);

    std::remove(sbom_path.c_str());
}

/* ========================================================================
 * Trusted Source Tests
 * ======================================================================== */

TEST_F(SupplyChainTest, AddTrustedSource) {
    EXPECT_EQ(nimcp_supply_chain_add_trusted_source(
                  sc, "https://trusted.example.com",
                  "/path/to/pubkey.pem", NIMCP_SIG_ED25519),
              NIMCP_SUCCESS);

    EXPECT_TRUE(nimcp_supply_chain_is_source_trusted(sc,
                                                       "https://trusted.example.com"));
}

TEST_F(SupplyChainTest, RevokeTrustedSource) {
    ASSERT_EQ(nimcp_supply_chain_add_trusted_source(
                  sc, "https://revoke.example.com",
                  "/path/to/key.pem", NIMCP_SIG_RSA_2048),
              NIMCP_SUCCESS);

    EXPECT_TRUE(nimcp_supply_chain_is_source_trusted(sc,
                                                       "https://revoke.example.com"));

    EXPECT_EQ(nimcp_supply_chain_revoke_source(sc, "https://revoke.example.com"),
              NIMCP_SUCCESS);

    EXPECT_FALSE(nimcp_supply_chain_is_source_trusted(sc,
                                                        "https://revoke.example.com"));
}

TEST_F(SupplyChainTest, ListTrustedSources) {
    // Add multiple sources
    nimcp_supply_chain_add_trusted_source(sc, "https://source1.com",
                                           "/key1.pem", NIMCP_SIG_ED25519);
    nimcp_supply_chain_add_trusted_source(sc, "https://source2.com",
                                           "/key2.pem", NIMCP_SIG_RSA_4096);

    nimcp_trusted_source_t* sources = nullptr;
    size_t count = 0;
    EXPECT_EQ(nimcp_supply_chain_list_sources(sc, &sources, &count), NIMCP_SUCCESS);
    EXPECT_GE(count, 2);
    EXPECT_NE(sources, nullptr);

    free(sources);
}

TEST_F(SupplyChainTest, UntrustedSource) {
    EXPECT_FALSE(nimcp_supply_chain_is_source_trusted(sc,
                                                        "https://untrusted.example.com"));
}

/* ========================================================================
 * Runtime Verification Tests
 * ======================================================================== */

TEST_F(SupplyChainTest, VerifyLibrary) {
    EXPECT_EQ(nimcp_runtime_verify_library(sc, "/lib/x86_64-linux-gnu/libc.so.6"),
              NIMCP_SUCCESS);

    nimcp_supply_chain_stats_t stats;
    nimcp_supply_chain_get_stats(sc, &stats);
    EXPECT_GT(stats.runtime_checks, 0);
}

TEST_F(SupplyChainTest, VerifyAllLibraries) {
    EXPECT_EQ(nimcp_runtime_verify_all(sc), NIMCP_SUCCESS);

    nimcp_supply_chain_stats_t stats;
    nimcp_supply_chain_get_stats(sc, &stats);
    EXPECT_GT(stats.runtime_checks, 0);
}

TEST_F(SupplyChainTest, EnableDisableMonitoring) {
    EXPECT_EQ(nimcp_runtime_enable_monitoring(sc), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_runtime_disable_monitoring(sc), NIMCP_SUCCESS);
}

TEST_F(SupplyChainTest, VerifyBinary) {
    EXPECT_EQ(nimcp_runtime_verify_binary(sc, "/bin/ls"), NIMCP_SUCCESS);
}

/* ========================================================================
 * Report Generation Tests
 * ======================================================================== */

TEST_F(SupplyChainTest, ExportReport) {
    char* report = nullptr;
    EXPECT_EQ(nimcp_supply_chain_export_report(sc, "text", &report), NIMCP_SUCCESS);
    EXPECT_NE(report, nullptr);

    // Check report contains expected sections
    EXPECT_NE(strstr(report, "Supply Chain Security Report"), nullptr);
    EXPECT_NE(strstr(report, "Total Dependencies"), nullptr);

    free(report);
}

/* ========================================================================
 * Invalid Argument Tests
 * ======================================================================== */

TEST_F(SupplyChainTest, InvalidArguments) {
    EXPECT_EQ(nimcp_supply_chain_get_stats(nullptr, nullptr),
              NIMCP_ERROR_INVALID_PARAMETER);

    nimcp_supply_chain_stats_t stats;
    EXPECT_EQ(nimcp_supply_chain_get_stats(nullptr, &stats),
              NIMCP_ERROR_INVALID_PARAMETER);

    EXPECT_EQ(nimcp_sbom_add_dependency(nullptr, nullptr),
              NIMCP_ERROR_INVALID_PARAMETER);

    EXPECT_EQ(nimcp_artifact_compute_hash(nullptr, nullptr,
                                           NIMCP_HASH_SHA256, nullptr),
              NIMCP_ERROR_INVALID_PARAMETER);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
