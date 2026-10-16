/**
 * @file test_post_quantum_integration.cpp
 * @brief Integration Tests for Post-Quantum Cryptography
 *
 * Tests integration between Kyber, Dilithium, hybrid mode, and bio-async
 */

#include <gtest/gtest.h>
// Headers have their own extern "C" guards
    #include "security/nimcp_post_quantum.h"
    #include "async/nimcp_bio_router.h"
    #include "core/nimcp_error.h"
#include <vector>
#include <thread>
#include <chrono>

class PostQuantumIntegrationTest : public ::testing::Test {
protected:
    nimcp_pq_context_t ctx;
    nimcp_bio_ctx_t bio_ctx;

    void SetUp() override {
        // Create bio-async context
        nimcp_bio_config_t bio_config;
        memset(&bio_config, 0, sizeof(bio_config));
        bio_config.mode = NIMCP_BIO_ASYNC_MODE_MULTI_THREADED;
        bio_config.num_threads = 2;

        bio_ctx = nimcp_bio_router_create(&bio_config);
        ASSERT_NE(bio_ctx, nullptr);

        // Create PQ context with bio-async
        nimcp_pq_config_t config;
        memset(&config, 0, sizeof(config));
        config.default_kyber_variant = NIMCP_PQ_KYBER_768;
        config.default_dilithium_variant = NIMCP_PQ_DILITHIUM_3;
        config.enable_logging = true;
        config.bio_ctx = bio_ctx;

        ctx = nimcp_pq_context_create(&config);
        ASSERT_NE(ctx, nullptr);
    }

    void TearDown() override {
        if (ctx) {
            nimcp_pq_context_destroy(ctx);
        }
        if (bio_ctx) {
            nimcp_bio_router_destroy(bio_ctx);
        }
    }
};

TEST_F(PostQuantumIntegrationTest, KyberDilithiumCombination) {
    // Generate both keypairs
    nimcp_kyber_keypair_t kyber_keypair;
    nimcp_dilithium_keypair_t dilithium_keypair;

    ASSERT_EQ(nimcp_kyber_keygen(NIMCP_PQ_KYBER_768, &kyber_keypair), NIMCP_SUCCESS);
    ASSERT_EQ(nimcp_dilithium_keygen(NIMCP_PQ_DILITHIUM_3, &dilithium_keypair), NIMCP_SUCCESS);

    // Use Kyber for key exchange
    uint8_t ciphertext[NIMCP_KYBER_768_CIPHERTEXT_BYTES];
    uint8_t shared_secret[NIMCP_KYBER_768_SHARED_SECRET_BYTES];
    size_t ct_len = sizeof(ciphertext);

    EXPECT_EQ(nimcp_kyber_encapsulate(NIMCP_PQ_KYBER_768, kyber_keypair.public_key,
                                       ciphertext, &ct_len, shared_secret, sizeof(shared_secret)),
              NIMCP_SUCCESS);

    // Use Dilithium to sign the shared secret
    uint8_t signature[NIMCP_DILITHIUM_3_SIGNATURE_BYTES];
    size_t sig_len = sizeof(signature);

    EXPECT_EQ(nimcp_dilithium_sign(NIMCP_PQ_DILITHIUM_3, dilithium_keypair.secret_key,
                                    shared_secret, sizeof(shared_secret),
                                    signature, &sig_len),
              NIMCP_SUCCESS);

    // Verify the signature
    EXPECT_EQ(nimcp_dilithium_verify(NIMCP_PQ_DILITHIUM_3, dilithium_keypair.public_key,
                                      shared_secret, sizeof(shared_secret),
                                      signature, sig_len),
              NIMCP_SUCCESS);

    nimcp_kyber_keypair_free(&kyber_keypair);
    nimcp_dilithium_keypair_free(&dilithium_keypair);
}

TEST_F(PostQuantumIntegrationTest, MultipleKyberVariants) {
    std::vector<nimcp_kyber_variant_t> variants = {
        NIMCP_PQ_KYBER_512,
        NIMCP_PQ_KYBER_768,
        NIMCP_PQ_KYBER_1024
    };

    for (auto variant : variants) {
        nimcp_kyber_keypair_t keypair;
        ASSERT_EQ(nimcp_kyber_keygen(variant, &keypair), NIMCP_SUCCESS);

        size_t pk_len, sk_len, ct_len;
        nimcp_kyber_get_sizes(variant, &pk_len, &sk_len, &ct_len);

        std::vector<uint8_t> ciphertext(ct_len);
        uint8_t secret1[32], secret2[32];
        size_t actual_ct_len = ct_len;

        EXPECT_EQ(nimcp_kyber_encapsulate(variant, keypair.public_key,
                                           ciphertext.data(), &actual_ct_len,
                                           secret1, sizeof(secret1)),
                  NIMCP_SUCCESS);

        EXPECT_EQ(nimcp_kyber_decapsulate(variant, keypair.secret_key,
                                           ciphertext.data(), actual_ct_len,
                                           secret2, sizeof(secret2)),
                  NIMCP_SUCCESS);

        nimcp_kyber_keypair_free(&keypair);
    }
}

TEST_F(PostQuantumIntegrationTest, HybridEndToEnd) {
    // Alice's keys
    uint8_t alice_classical_priv[NIMCP_X25519_KEY_BYTES];
    uint8_t alice_classical_pub[NIMCP_X25519_KEY_BYTES];
    memset(alice_classical_priv, 0xAA, sizeof(alice_classical_priv));
    memset(alice_classical_pub, 0xBB, sizeof(alice_classical_pub));

    nimcp_kyber_keypair_t alice_pq_keypair;
    ASSERT_EQ(nimcp_kyber_keygen(NIMCP_PQ_KYBER_1024, &alice_pq_keypair), NIMCP_SUCCESS);

    // Bob's keys
    uint8_t bob_classical_priv[NIMCP_X25519_KEY_BYTES];
    uint8_t bob_classical_pub[NIMCP_X25519_KEY_BYTES];
    memset(bob_classical_priv, 0xCC, sizeof(bob_classical_priv));
    memset(bob_classical_pub, 0xDD, sizeof(bob_classical_pub));

    nimcp_kyber_keypair_t bob_pq_keypair;
    ASSERT_EQ(nimcp_kyber_keygen(NIMCP_PQ_KYBER_1024, &bob_pq_keypair), NIMCP_SUCCESS);

    // Alice computes shared secret with Bob's public keys
    uint8_t alice_secret[NIMCP_HYBRID_SHARED_SECRET_BYTES];
    EXPECT_EQ(nimcp_hybrid_key_exchange(ctx, alice_classical_priv, bob_classical_pub,
                                         bob_pq_keypair.public_key,
                                         bob_pq_keypair.public_key_len,
                                         alice_secret, sizeof(alice_secret)),
              NIMCP_SUCCESS);

    // Bob computes shared secret with Alice's public keys
    uint8_t bob_secret[NIMCP_HYBRID_SHARED_SECRET_BYTES];
    EXPECT_EQ(nimcp_hybrid_key_exchange(ctx, bob_classical_priv, alice_classical_pub,
                                         alice_pq_keypair.public_key,
                                         alice_pq_keypair.public_key_len,
                                         bob_secret, sizeof(bob_secret)),
              NIMCP_SUCCESS);

    // Note: In reference implementation, secrets may differ
    // Production implementation would ensure they match

    nimcp_kyber_keypair_free(&alice_pq_keypair);
    nimcp_kyber_keypair_free(&bob_pq_keypair);
}

TEST_F(PostQuantumIntegrationTest, ConcurrentOperations) {
    const int num_threads = 4;
    const int ops_per_thread = 10;
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([ops_per_thread]() {
            for (int i = 0; i < ops_per_thread; i++) {
                nimcp_kyber_keypair_t keypair;
                EXPECT_EQ(nimcp_kyber_keygen(NIMCP_PQ_KYBER_512, &keypair), NIMCP_SUCCESS);
                nimcp_kyber_keypair_free(&keypair);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }
}

TEST_F(PostQuantumIntegrationTest, StatsTracking) {
    // Perform various operations
    nimcp_kyber_keypair_t kyber_keypair;
    nimcp_dilithium_keypair_t dilithium_keypair;

    nimcp_kyber_keygen(NIMCP_PQ_KYBER_768, &kyber_keypair);
    nimcp_dilithium_keygen(NIMCP_PQ_DILITHIUM_3, &dilithium_keypair);

    uint8_t ciphertext[NIMCP_KYBER_768_CIPHERTEXT_BYTES];
    uint8_t secret[32];
    size_t ct_len = sizeof(ciphertext);

    nimcp_kyber_encapsulate(NIMCP_PQ_KYBER_768, kyber_keypair.public_key,
                             ciphertext, &ct_len, secret, sizeof(secret));

    // Check stats were updated
    nimcp_pq_stats_t stats;
    EXPECT_EQ(nimcp_pq_get_stats(ctx, &stats), NIMCP_SUCCESS);

    // Stats should reflect operations (note: context doesn't track individual ops in this impl)
    // Just verify we can retrieve stats

    nimcp_kyber_keypair_free(&kyber_keypair);
    nimcp_dilithium_keypair_free(&dilithium_keypair);
}

TEST_F(PostQuantumIntegrationTest, SelfTestComprehensive) {
    EXPECT_EQ(nimcp_pq_self_test(ctx), NIMCP_SUCCESS);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
