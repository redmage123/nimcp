/**
 * @file test_supply_chain_integration.cpp
 * @brief Integration Tests for Supply Chain Security
 */

#include <gtest/gtest.h>
// Headers have their own extern "C" guards
    #include "security/nimcp_supply_chain.h"
    #include "security/nimcp_post_quantum.h"
    #include "async/nimcp_bio_router.h"
#include <fstream>
#include <string>

class SupplyChainIntegrationTest : public ::testing::Test {
protected:
    nimcp_supply_chain_t sc;
    nimcp_bio_ctx_t bio_ctx;

    void SetUp() override {
        nimcp_bio_config_t bio_config;
        memset(&bio_config, 0, sizeof(bio_config));
        bio_config.mode = NIMCP_BIO_ASYNC_MODE_MULTI_THREADED;
        bio_config.num_threads = 2;

        bio_ctx = nimcp_bio_router_create(&bio_config);

        nimcp_supply_chain_config_t config;
        memset(&config, 0, sizeof(config));
        config.enable_logging = true;
        config.bio_ctx = bio_ctx;

        sc = nimcp_supply_chain_create(&config);
        ASSERT_NE(sc, nullptr);
    }

    void TearDown() override {
        if (sc) nimcp_supply_chain_destroy(sc);
        if (bio_ctx) nimcp_bio_router_destroy(bio_ctx);
    }
};

TEST_F(SupplyChainIntegrationTest, SBOMWorkflow) {
    // Add dependencies
    for (int i = 0; i < 10; i++) {
        nimcp_dependency_t dep;
        memset(&dep, 0, sizeof(dep));
        snprintf(dep.name, sizeof(dep.name), "library-%d", i);
        snprintf(dep.version, sizeof(dep.version), "1.%d.0", i);
        ASSERT_EQ(nimcp_sbom_add_dependency(sc, &dep), NIMCP_SUCCESS);
    }

    // Generate and save SBOM
    char* sbom = nullptr;
    EXPECT_EQ(nimcp_sbom_generate(sc, NIMCP_SBOM_FORMAT_SPDX, &sbom), NIMCP_SUCCESS);
    EXPECT_NE(sbom, nullptr);
    free(sbom);

    // Export report
    char* report = nullptr;
    EXPECT_EQ(nimcp_supply_chain_export_report(sc, "text", &report), NIMCP_SUCCESS);
    EXPECT_NE(report, nullptr);
    free(report);
}

TEST_F(SupplyChainIntegrationTest, MultipleHashVerifications) {
    std::string test_file = "/tmp/nimcp_multi_hash_test.txt";
    std::ofstream file(test_file);
    file << "Test content\n";
    file.close();

    // Compute hashes with different algorithms
    char sha256[129], sha512[129];

    EXPECT_EQ(nimcp_artifact_compute_hash(sc, test_file.c_str(),
                                           NIMCP_HASH_SHA256, sha256), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_artifact_compute_hash(sc, test_file.c_str(),
                                           NIMCP_HASH_SHA512, sha512), NIMCP_SUCCESS);

    // Verify both hashes
    EXPECT_EQ(nimcp_artifact_verify_hash(sc, test_file.c_str(),
                                          sha256, NIMCP_HASH_SHA256), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_artifact_verify_hash(sc, test_file.c_str(),
                                          sha512, NIMCP_HASH_SHA512), NIMCP_SUCCESS);

    std::remove(test_file.c_str());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
