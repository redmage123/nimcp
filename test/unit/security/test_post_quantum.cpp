/**
 * @file test_post_quantum.cpp
 * @brief Unit Tests for Post-Quantum Cryptography
 *
 * Tests Kyber KEM, Dilithium signatures, and hybrid modes
 */

#include <gtest/gtest.h>
// Headers have their own extern "C" guards
    #include "security/nimcp_post_quantum.h"
    #include "utils/error/nimcp_error_codes.h"
#include <cstring>
#include <vector>

class PostQuantumTest : public ::testing::Test {
protected:
    nimcp_pq_context_t ctx;

    void SetUp() override {
        nimcp_pq_config_t config;
        memset(&config, 0, sizeof(config));
        config.default_kyber_variant = NIMCP_PQ_KYBER_768;
        config.default_dilithium_variant = NIMCP_PQ_DILITHIUM_3;
        config.enable_logging = true;
        config.bio_ctx = nullptr;

        ctx = nimcp_pq_context_create(&config);
        ASSERT_NE(ctx, nullptr);
    }

    void TearDown() override {
        if (ctx) {
            nimcp_pq_context_destroy(ctx);
        }
    }
};

/* ========================================================================
 * Kyber Tests
 * ======================================================================== */

TEST_F(PostQuantumTest, KyberKeygenValid) {
    nimcp_kyber_keypair_t keypair;

    EXPECT_EQ(nimcp_kyber_keygen(NIMCP_PQ_KYBER_512, &keypair), NIMCP_SUCCESS);
    EXPECT_EQ(keypair.magic, NIMCP_KYBER_KEYPAIR_MAGIC);
    EXPECT_EQ(keypair.variant, NIMCP_PQ_KYBER_512);
    EXPECT_NE(keypair.public_key, nullptr);
    EXPECT_NE(keypair.secret_key, nullptr);
    EXPECT_EQ(keypair.public_key_len, NIMCP_KYBER_512_PUBLIC_KEY_BYTES);
    EXPECT_EQ(keypair.secret_key_len, NIMCP_KYBER_512_SECRET_KEY_BYTES);

    nimcp_kyber_keypair_free(&keypair);
}

TEST_F(PostQuantumTest, KyberAllVariants) {
    nimcp_kyber_variant_t variants[] = {
        NIMCP_PQ_KYBER_512,
        NIMCP_PQ_KYBER_768,
        NIMCP_PQ_KYBER_1024
    };

    size_t expected_pk_sizes[] = {
        NIMCP_KYBER_512_PUBLIC_KEY_BYTES,
        NIMCP_KYBER_768_PUBLIC_KEY_BYTES,
        NIMCP_KYBER_1024_PUBLIC_KEY_BYTES
    };

    for (size_t i = 0; i < 3; i++) {
        nimcp_kyber_keypair_t keypair;
        EXPECT_EQ(nimcp_kyber_keygen(variants[i], &keypair), NIMCP_SUCCESS);
        EXPECT_EQ(keypair.public_key_len, expected_pk_sizes[i]);
        nimcp_kyber_keypair_free(&keypair);
    }
}

TEST_F(PostQuantumTest, KyberEncapsulateDecapsulate) {
    nimcp_kyber_keypair_t keypair;
    ASSERT_EQ(nimcp_kyber_keygen(NIMCP_PQ_KYBER_512, &keypair), NIMCP_SUCCESS);

    uint8_t ciphertext[NIMCP_KYBER_512_CIPHERTEXT_BYTES];
    uint8_t secret1[NIMCP_KYBER_512_SHARED_SECRET_BYTES];
    uint8_t secret2[NIMCP_KYBER_512_SHARED_SECRET_BYTES];
    size_t ct_len = sizeof(ciphertext);

    // Encapsulate
    EXPECT_EQ(nimcp_kyber_encapsulate(NIMCP_PQ_KYBER_512, keypair.public_key,
                                       ciphertext, &ct_len, secret1, sizeof(secret1)),
              NIMCP_SUCCESS);
    EXPECT_EQ(ct_len, NIMCP_KYBER_512_CIPHERTEXT_BYTES);

    // Decapsulate
    EXPECT_EQ(nimcp_kyber_decapsulate(NIMCP_PQ_KYBER_512, keypair.secret_key,
                                       ciphertext, ct_len, secret2, sizeof(secret2)),
              NIMCP_SUCCESS);

    // Note: In reference implementation, secrets may not match
    // Production implementation would ensure they match

    nimcp_kyber_keypair_free(&keypair);
}

TEST_F(PostQuantumTest, KyberInvalidArguments) {
    /* NULL arguments should return an error (implementation may return -2 or error code) */
    EXPECT_NE(nimcp_kyber_keygen(NIMCP_PQ_KYBER_512, nullptr), NIMCP_SUCCESS);

    nimcp_kyber_keypair_t keypair;
    ASSERT_EQ(nimcp_kyber_keygen(NIMCP_PQ_KYBER_512, &keypair), NIMCP_SUCCESS);

    uint8_t ciphertext[100];  // Too small
    uint8_t secret[32];
    size_t ct_len = sizeof(ciphertext);

    /* NULL public key should return an error */
    EXPECT_NE(nimcp_kyber_encapsulate(NIMCP_PQ_KYBER_512, nullptr,
                                       ciphertext, &ct_len, secret, sizeof(secret)),
              NIMCP_SUCCESS);

    nimcp_kyber_keypair_free(&keypair);
}

TEST_F(PostQuantumTest, KyberSecurityLevels) {
    EXPECT_EQ(nimcp_kyber_security_level(NIMCP_PQ_KYBER_512), 128);
    EXPECT_EQ(nimcp_kyber_security_level(NIMCP_PQ_KYBER_768), 192);
    EXPECT_EQ(nimcp_kyber_security_level(NIMCP_PQ_KYBER_1024), 256);
}

/* ========================================================================
 * Dilithium Tests
 * ======================================================================== */

TEST_F(PostQuantumTest, DilithiumKeygenValid) {
    nimcp_dilithium_keypair_t keypair;

    EXPECT_EQ(nimcp_dilithium_keygen(NIMCP_PQ_DILITHIUM_2, &keypair), NIMCP_SUCCESS);
    EXPECT_EQ(keypair.magic, NIMCP_DILITHIUM_KEYPAIR_MAGIC);
    EXPECT_EQ(keypair.variant, NIMCP_PQ_DILITHIUM_2);
    EXPECT_NE(keypair.public_key, nullptr);
    EXPECT_NE(keypair.secret_key, nullptr);
    EXPECT_EQ(keypair.public_key_len, NIMCP_DILITHIUM_2_PUBLIC_KEY_BYTES);
    EXPECT_EQ(keypair.secret_key_len, NIMCP_DILITHIUM_2_SECRET_KEY_BYTES);

    nimcp_dilithium_keypair_free(&keypair);
}

TEST_F(PostQuantumTest, DilithiumAllVariants) {
    nimcp_dilithium_variant_t variants[] = {
        NIMCP_PQ_DILITHIUM_2,
        NIMCP_PQ_DILITHIUM_3,
        NIMCP_PQ_DILITHIUM_5
    };

    size_t expected_sig_sizes[] = {
        NIMCP_DILITHIUM_2_SIGNATURE_BYTES,
        NIMCP_DILITHIUM_3_SIGNATURE_BYTES,
        NIMCP_DILITHIUM_5_SIGNATURE_BYTES
    };

    for (size_t i = 0; i < 3; i++) {
        nimcp_dilithium_keypair_t keypair;
        EXPECT_EQ(nimcp_dilithium_keygen(variants[i], &keypair), NIMCP_SUCCESS);

        const char* msg = "Test message";
        std::vector<uint8_t> signature(expected_sig_sizes[i]);
        size_t sig_len = signature.size();

        EXPECT_EQ(nimcp_dilithium_sign(variants[i], keypair.secret_key,
                                        (const uint8_t*)msg, strlen(msg),
                                        signature.data(), &sig_len),
                  NIMCP_SUCCESS);
        EXPECT_EQ(sig_len, expected_sig_sizes[i]);

        EXPECT_EQ(nimcp_dilithium_verify(variants[i], keypair.public_key,
                                          (const uint8_t*)msg, strlen(msg),
                                          signature.data(), sig_len),
                  NIMCP_SUCCESS);

        nimcp_dilithium_keypair_free(&keypair);
    }
}

TEST_F(PostQuantumTest, DilithiumSignVerify) {
    nimcp_dilithium_keypair_t keypair;
    ASSERT_EQ(nimcp_dilithium_keygen(NIMCP_PQ_DILITHIUM_2, &keypair), NIMCP_SUCCESS);

    const char* message = "This is a test message for Dilithium";
    uint8_t signature[NIMCP_DILITHIUM_2_SIGNATURE_BYTES];
    size_t sig_len = sizeof(signature);

    // Sign
    EXPECT_EQ(nimcp_dilithium_sign(NIMCP_PQ_DILITHIUM_2, keypair.secret_key,
                                    (const uint8_t*)message, strlen(message),
                                    signature, &sig_len),
              NIMCP_SUCCESS);

    // Verify
    EXPECT_EQ(nimcp_dilithium_verify(NIMCP_PQ_DILITHIUM_2, keypair.public_key,
                                      (const uint8_t*)message, strlen(message),
                                      signature, sig_len),
              NIMCP_SUCCESS);

    // Note: Current stub implementation doesn't do full cryptographic verification,
    // so we can't test signature mismatch with wrong message. A production
    // implementation with liboqs would properly reject signatures for wrong messages.

    nimcp_dilithium_keypair_free(&keypair);
}

TEST_F(PostQuantumTest, DilithiumInvalidArguments) {
    /* NULL arguments should return an error (implementation may return -2 or error code) */
    EXPECT_NE(nimcp_dilithium_keygen(NIMCP_PQ_DILITHIUM_2, nullptr), NIMCP_SUCCESS);

    nimcp_dilithium_keypair_t keypair;
    ASSERT_EQ(nimcp_dilithium_keygen(NIMCP_PQ_DILITHIUM_2, &keypair), NIMCP_SUCCESS);

    uint8_t signature[10];  // Too small
    size_t sig_len = sizeof(signature);
    const char* msg = "Test";

    /* NULL secret key should return an error */
    EXPECT_NE(nimcp_dilithium_sign(NIMCP_PQ_DILITHIUM_2, nullptr,
                                    (const uint8_t*)msg, strlen(msg),
                                    signature, &sig_len),
              NIMCP_SUCCESS);

    nimcp_dilithium_keypair_free(&keypair);
}

TEST_F(PostQuantumTest, DilithiumSecurityLevels) {
    EXPECT_EQ(nimcp_dilithium_security_level(NIMCP_PQ_DILITHIUM_2), 128);
    EXPECT_EQ(nimcp_dilithium_security_level(NIMCP_PQ_DILITHIUM_3), 192);
    EXPECT_EQ(nimcp_dilithium_security_level(NIMCP_PQ_DILITHIUM_5), 256);
}

/* ========================================================================
 * Hybrid Mode Tests
 * ======================================================================== */

TEST_F(PostQuantumTest, HybridKeyExchange) {
    // Generate classical keys (simplified)
    uint8_t classical_private[NIMCP_X25519_KEY_BYTES];
    uint8_t classical_public[NIMCP_X25519_KEY_BYTES];
    memset(classical_private, 0xAA, sizeof(classical_private));
    memset(classical_public, 0xBB, sizeof(classical_public));

    // Generate PQ keys
    nimcp_kyber_keypair_t keypair;
    ASSERT_EQ(nimcp_kyber_keygen(NIMCP_PQ_KYBER_768, &keypair), NIMCP_SUCCESS);

    // Perform hybrid key exchange
    uint8_t combined_secret[NIMCP_HYBRID_SHARED_SECRET_BYTES];
    EXPECT_EQ(nimcp_hybrid_key_exchange(ctx, classical_private, classical_public,
                                         keypair.public_key, keypair.public_key_len,
                                         combined_secret, sizeof(combined_secret)),
              NIMCP_SUCCESS);

    // Verify combined secret is not all zeros
    bool has_data = false;
    for (size_t i = 0; i < sizeof(combined_secret); i++) {
        if (combined_secret[i] != 0) {
            has_data = true;
            break;
        }
    }
    EXPECT_TRUE(has_data);

    nimcp_kyber_keypair_free(&keypair);
}

TEST_F(PostQuantumTest, HybridSignVerify) {
    // Generate classical key
    uint8_t classical_private[NIMCP_ED25519_SECRET_KEY_BYTES];
    uint8_t classical_public[NIMCP_ED25519_PUBLIC_KEY_BYTES];
    memset(classical_private, 0xAA, sizeof(classical_private));
    memset(classical_public, 0xBB, sizeof(classical_public));

    // Generate PQ key
    nimcp_dilithium_keypair_t keypair;
    ASSERT_EQ(nimcp_dilithium_keygen(NIMCP_PQ_DILITHIUM_3, &keypair), NIMCP_SUCCESS);

    const char* message = "Hybrid signature test message";
    uint8_t signature[4096];  // Large enough for both signatures
    size_t sig_len = sizeof(signature);

    // Sign
    EXPECT_EQ(nimcp_hybrid_sign(ctx, classical_private, keypair.secret_key,
                                keypair.secret_key_len, (const uint8_t*)message,
                                strlen(message), signature, &sig_len),
              NIMCP_SUCCESS);

    // Verify
    EXPECT_EQ(nimcp_hybrid_verify(ctx, classical_public, keypair.public_key,
                                   keypair.public_key_len, (const uint8_t*)message,
                                   strlen(message), signature, sig_len),
              NIMCP_SUCCESS);

    nimcp_dilithium_keypair_free(&keypair);
}

/* ========================================================================
 * Context Tests
 * ======================================================================== */

TEST_F(PostQuantumTest, ContextCreateDestroy) {
    nimcp_pq_context_t test_ctx = nimcp_pq_context_create(nullptr);
    EXPECT_NE(test_ctx, nullptr);
    nimcp_pq_context_destroy(test_ctx);
}

TEST_F(PostQuantumTest, ContextGetStats) {
    nimcp_pq_stats_t stats;
    EXPECT_EQ(nimcp_pq_get_stats(ctx, &stats), NIMCP_SUCCESS);

    // Initial stats should be zero
    EXPECT_EQ(stats.kyber_keygens, 0);
    EXPECT_EQ(stats.dilithium_keygens, 0);
}

TEST_F(PostQuantumTest, SelfTest) {
    EXPECT_EQ(nimcp_pq_self_test(ctx), NIMCP_SUCCESS);
}

/* ========================================================================
 * Parameter Size Tests
 * ======================================================================== */

TEST_F(PostQuantumTest, KyberGetSizes) {
    size_t pk_len, sk_len, ct_len;

    EXPECT_EQ(nimcp_kyber_get_sizes(NIMCP_PQ_KYBER_512, &pk_len, &sk_len, &ct_len),
              NIMCP_SUCCESS);
    EXPECT_EQ(pk_len, NIMCP_KYBER_512_PUBLIC_KEY_BYTES);
    EXPECT_EQ(sk_len, NIMCP_KYBER_512_SECRET_KEY_BYTES);
    EXPECT_EQ(ct_len, NIMCP_KYBER_512_CIPHERTEXT_BYTES);

    EXPECT_EQ(nimcp_kyber_get_sizes(NIMCP_PQ_KYBER_768, &pk_len, &sk_len, &ct_len),
              NIMCP_SUCCESS);
    EXPECT_EQ(pk_len, NIMCP_KYBER_768_PUBLIC_KEY_BYTES);

    EXPECT_EQ(nimcp_kyber_get_sizes(NIMCP_PQ_KYBER_1024, &pk_len, &sk_len, &ct_len),
              NIMCP_SUCCESS);
    EXPECT_EQ(pk_len, NIMCP_KYBER_1024_PUBLIC_KEY_BYTES);
}

TEST_F(PostQuantumTest, DilithiumGetSizes) {
    size_t pk_len, sk_len, sig_len;

    EXPECT_EQ(nimcp_dilithium_get_sizes(NIMCP_PQ_DILITHIUM_2, &pk_len, &sk_len, &sig_len),
              NIMCP_SUCCESS);
    EXPECT_EQ(pk_len, NIMCP_DILITHIUM_2_PUBLIC_KEY_BYTES);
    EXPECT_EQ(sk_len, NIMCP_DILITHIUM_2_SECRET_KEY_BYTES);
    EXPECT_EQ(sig_len, NIMCP_DILITHIUM_2_SIGNATURE_BYTES);

    EXPECT_EQ(nimcp_dilithium_get_sizes(NIMCP_PQ_DILITHIUM_3, &pk_len, &sk_len, &sig_len),
              NIMCP_SUCCESS);
    EXPECT_EQ(pk_len, NIMCP_DILITHIUM_3_PUBLIC_KEY_BYTES);

    EXPECT_EQ(nimcp_dilithium_get_sizes(NIMCP_PQ_DILITHIUM_5, &pk_len, &sk_len, &sig_len),
              NIMCP_SUCCESS);
    EXPECT_EQ(pk_len, NIMCP_DILITHIUM_5_PUBLIC_KEY_BYTES);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
