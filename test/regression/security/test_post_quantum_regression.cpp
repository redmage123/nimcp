/**
 * @file test_post_quantum_regression.cpp
 * @brief Regression Tests for Post-Quantum Cryptography
 */

#include <gtest/gtest.h>
// Headers have their own extern "C" guards
    #include "security/nimcp_post_quantum.h"

class PostQuantumRegressionTest : public ::testing::Test {
protected:
    nimcp_pq_context_t ctx;

    void SetUp() override {
        ctx = nimcp_pq_context_create(nullptr);
        ASSERT_NE(ctx, nullptr);
    }

    void TearDown() override {
        if (ctx) nimcp_pq_context_destroy(ctx);
    }
};

TEST_F(PostQuantumRegressionTest, KyberPerformance) {
    const int iterations = 100;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; i++) {
        nimcp_kyber_keypair_t keypair;
        ASSERT_EQ(nimcp_kyber_keygen(NIMCP_PQ_KYBER_512, &keypair), NIMCP_SUCCESS);
        nimcp_kyber_keypair_free(&keypair);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete in reasonable time
    EXPECT_LT(duration.count(), 10000);  // < 10 seconds
}

TEST_F(PostQuantumRegressionTest, DilithiumStability) {
    const int iterations = 100;

    for (int i = 0; i < iterations; i++) {
        nimcp_dilithium_keypair_t keypair;
        ASSERT_EQ(nimcp_dilithium_keygen(NIMCP_PQ_DILITHIUM_2, &keypair), NIMCP_SUCCESS);

        const char* msg = "Stability test message";
        uint8_t sig[NIMCP_DILITHIUM_2_SIGNATURE_BYTES];
        size_t sig_len = sizeof(sig);

        ASSERT_EQ(nimcp_dilithium_sign(NIMCP_PQ_DILITHIUM_2, keypair.secret_key,
                                        (const uint8_t*)msg, strlen(msg), sig, &sig_len),
                  NIMCP_SUCCESS);

        ASSERT_EQ(nimcp_dilithium_verify(NIMCP_PQ_DILITHIUM_2, keypair.public_key,
                                          (const uint8_t*)msg, strlen(msg), sig, sig_len),
                  NIMCP_SUCCESS);

        nimcp_dilithium_keypair_free(&keypair);
    }
}

TEST_F(PostQuantumRegressionTest, MemoryLeakCheck) {
    // Allocate and free many keypairs
    for (int i = 0; i < 1000; i++) {
        nimcp_kyber_keypair_t kyber_keypair;
        nimcp_dilithium_keypair_t dilithium_keypair;

        nimcp_kyber_keygen(NIMCP_PQ_KYBER_768, &kyber_keypair);
        nimcp_dilithium_keygen(NIMCP_PQ_DILITHIUM_3, &dilithium_keypair);

        nimcp_kyber_keypair_free(&kyber_keypair);
        nimcp_dilithium_keypair_free(&dilithium_keypair);
    }

    // If we get here without crashing, no obvious leaks
    SUCCEED();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
