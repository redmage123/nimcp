/**
 * @file test_supply_chain_regression.cpp
 * @brief Regression Tests for Supply Chain Security
 */

#include <gtest/gtest.h>
// Headers have their own extern "C" guards
    #include "security/nimcp_supply_chain.h"
#include <fstream>

class SupplyChainRegressionTest : public ::testing::Test {
protected:
    nimcp_supply_chain_t sc;

    void SetUp() override {
        sc = nimcp_supply_chain_create(nullptr);
        ASSERT_NE(sc, nullptr);
    }

    void TearDown() override {
        if (sc) nimcp_supply_chain_destroy(sc);
    }
};

TEST_F(SupplyChainRegressionTest, LargeSBOM) {
    // Add many dependencies
    const int num_deps = 1000;

    for (int i = 0; i < num_deps; i++) {
        nimcp_dependency_t dep;
        memset(&dep, 0, sizeof(dep));
        snprintf(dep.name, sizeof(dep.name), "library-%d", i);
        snprintf(dep.version, sizeof(dep.version), "1.%d.0", i % 10);
        ASSERT_EQ(nimcp_sbom_add_dependency(sc, &dep), NIMCP_SUCCESS);
    }

    // Generate SBOM
    char* sbom = nullptr;
    EXPECT_EQ(nimcp_sbom_generate(sc, NIMCP_SBOM_FORMAT_SPDX, &sbom), NIMCP_SUCCESS);
    EXPECT_NE(sbom, nullptr);

    free(sbom);
}

TEST_F(SupplyChainRegressionTest, HashPerformance) {
    // Create test file
    std::string test_file = "/tmp/nimcp_perf_test.bin";
    std::ofstream file(test_file, std::ios::binary);

    // Write 1MB of data
    std::vector<uint8_t> data(1024 * 1024, 0xAA);
    file.write(reinterpret_cast<char*>(data.data()), data.size());
    file.close();

    auto start = std::chrono::high_resolution_clock::now();

    // Compute hash 100 times
    for (int i = 0; i < 100; i++) {
        char hash[129];
        ASSERT_EQ(nimcp_artifact_compute_hash(sc, test_file.c_str(),
                                               NIMCP_HASH_SHA256, hash), NIMCP_SUCCESS);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete in reasonable time
    EXPECT_LT(duration.count(), 5000);  // < 5 seconds

    std::remove(test_file.c_str());
}

TEST_F(SupplyChainRegressionTest, ConcurrentOperations) {
    const int num_threads = 10;
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this]() {
            for (int i = 0; i < 100; i++) {
                nimcp_dependency_t dep;
                memset(&dep, 0, sizeof(dep));
                snprintf(dep.name, sizeof(dep.name), "concurrent-lib-%d", i);
                nimcp_sbom_add_dependency(sc, &dep);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Verify dependencies were added
    nimcp_dependency_t* deps = nullptr;
    size_t count = 0;
    EXPECT_EQ(nimcp_sbom_get_dependencies(sc, &deps, &count), NIMCP_SUCCESS);
    EXPECT_GT(count, 0);

    free(deps);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
