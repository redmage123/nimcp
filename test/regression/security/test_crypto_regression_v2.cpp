/**
 * @file test_crypto_regression_v2.cpp
 * @brief Comprehensive GTest Regression Tests for Post-Quantum Cryptography Module
 *
 * WHAT: GTest-based regression tests verifying post-quantum crypto backward compatibility
 * WHY:  Ensure cryptographic operations remain stable and secure across versions
 * HOW:  Test API contracts, key generation, algorithm consistency, and performance
 *
 * REGRESSION CATEGORIES:
 * 1. API Contract Stability - Function signatures, return values
 * 2. Kyber KEM Backward Compatibility - Key generation, encapsulation, decapsulation
 * 3. Dilithium Signatures - Sign/verify consistency
 * 4. Hash Algorithm Consistency - Same input produces same hash
 * 5. Key Generation Stability - Keys have expected properties
 * 6. Performance Baselines - Crypto operations stay within time bounds
 * 7. Security Level Consistency - NIST levels map correctly
 *
 * @author NIMCP Security Team
 * @date 2026-02-02
 */

#include "test_helpers.h"

extern "C" {
#include "security/nimcp_post_quantum.h"
}

#include <chrono>
#include <cstring>
#include <string>
#include <vector>

namespace {

//=============================================================================
// Test Fixture
//=============================================================================

class CryptoRegressionV2Test : public ::testing::Test {
protected:
    void SetUp() override
    {
        ctx_ = nimcp_pq_context_create(nullptr);
    }

    void TearDown() override
    {
        if (ctx_) {
            nimcp_pq_context_destroy(ctx_);
            ctx_ = nullptr;
        }
    }

    double get_time_ms()
    {
        auto now = std::chrono::high_resolution_clock::now();
        auto duration = now.time_since_epoch();
        return std::chrono::duration<double, std::milli>(duration).count();
    }

    nimcp_pq_context_t ctx_ = nullptr;
};

//=============================================================================
// API Contract Stability Tests
//=============================================================================

/**
 * Test: nimcp_pq_context_create() API contract
 * Regression: Must return valid context on NULL config (defaults)
 */
TEST_F(CryptoRegressionV2Test, ApiContextCreateContract)
{
    // NULL config should work (use defaults)
    nimcp_pq_context_t ctx1 = nimcp_pq_context_create(nullptr);
    EXPECT_NE(ctx1, nullptr) << "create(NULL) should succeed with defaults";
    nimcp_pq_context_destroy(ctx1);

    // Valid config should work
    nimcp_pq_config_t config = {};
    config.default_kyber_variant = NIMCP_PQ_KYBER_768;
    config.default_dilithium_variant = NIMCP_PQ_DILITHIUM_3;
    config.enable_logging = false;

    nimcp_pq_context_t ctx2 = nimcp_pq_context_create(&config);
    EXPECT_NE(ctx2, nullptr) << "create(&config) should succeed";
    nimcp_pq_context_destroy(ctx2);
}

/**
 * Test: nimcp_pq_context_destroy() API contract
 * Regression: Must handle NULL safely (no crash)
 */
TEST_F(CryptoRegressionV2Test, ApiContextDestroyNullSafety)
{
    // NULL should not crash
    nimcp_pq_context_destroy(nullptr);
    SUCCEED();
}

/**
 * Test: nimcp_kyber_keygen() return value contract
 * Regression: Must return NIMCP_OK on valid params, populate keypair
 */
TEST_F(CryptoRegressionV2Test, ApiKyberKeygenContract)
{
    nimcp_kyber_keypair_t keypair;
    memset(&keypair, 0, sizeof(keypair));

    nimcp_error_t result = nimcp_kyber_keygen(NIMCP_PQ_KYBER_512, &keypair);
    EXPECT_EQ(result, NIMCP_OK) << "keygen must return NIMCP_OK";
    EXPECT_EQ(keypair.magic, NIMCP_KYBER_KEYPAIR_MAGIC) << "keypair magic must be set";
    EXPECT_EQ(keypair.variant, NIMCP_PQ_KYBER_512) << "variant must match";
    EXPECT_NE(keypair.public_key, nullptr) << "public_key must not be NULL";
    EXPECT_NE(keypair.secret_key, nullptr) << "secret_key must not be NULL";
    EXPECT_EQ(keypair.public_key_len, static_cast<size_t>(NIMCP_KYBER_512_PUBLIC_KEY_BYTES))
        << "public_key_len must match";
    EXPECT_EQ(keypair.secret_key_len, static_cast<size_t>(NIMCP_KYBER_512_SECRET_KEY_BYTES))
        << "secret_key_len must match";

    nimcp_kyber_keypair_free(&keypair);

    // NULL keypair
    result = nimcp_kyber_keygen(NIMCP_PQ_KYBER_512, nullptr);
    EXPECT_NE(result, NIMCP_OK) << "keygen with NULL keypair must fail";
}

/**
 * Test: nimcp_dilithium_keygen() return value contract
 * Regression: Must return NIMCP_OK on valid params, populate keypair
 */
TEST_F(CryptoRegressionV2Test, ApiDilithiumKeygenContract)
{
    nimcp_dilithium_keypair_t keypair;
    memset(&keypair, 0, sizeof(keypair));

    nimcp_error_t result = nimcp_dilithium_keygen(NIMCP_PQ_DILITHIUM_2, &keypair);
    EXPECT_EQ(result, NIMCP_OK) << "keygen must return NIMCP_OK";
    EXPECT_EQ(keypair.magic, NIMCP_DILITHIUM_KEYPAIR_MAGIC) << "keypair magic must be set";
    EXPECT_EQ(keypair.variant, NIMCP_PQ_DILITHIUM_2) << "variant must match";
    EXPECT_NE(keypair.public_key, nullptr) << "public_key must not be NULL";
    EXPECT_NE(keypair.secret_key, nullptr) << "secret_key must not be NULL";
    EXPECT_EQ(keypair.public_key_len, static_cast<size_t>(NIMCP_DILITHIUM_2_PUBLIC_KEY_BYTES))
        << "public_key_len must match";
    EXPECT_EQ(keypair.secret_key_len, static_cast<size_t>(NIMCP_DILITHIUM_2_SECRET_KEY_BYTES))
        << "secret_key_len must match";

    nimcp_dilithium_keypair_free(&keypair);

    // NULL keypair
    result = nimcp_dilithium_keygen(NIMCP_PQ_DILITHIUM_2, nullptr);
    EXPECT_NE(result, NIMCP_OK) << "keygen with NULL keypair must fail";
}

//=============================================================================
// Enum and Constant Stability Tests
//=============================================================================

/**
 * Test: Kyber variant enum values must not change (ABI stability)
 * Regression: Changing enum values breaks serialization and compatibility
 */
TEST_F(CryptoRegressionV2Test, KyberEnumValuesStable)
{
    EXPECT_EQ(NIMCP_PQ_KYBER_512, 0) << "KYBER_512 must be 0";
    EXPECT_EQ(NIMCP_PQ_KYBER_768, 1) << "KYBER_768 must be 1";
    EXPECT_EQ(NIMCP_PQ_KYBER_1024, 2) << "KYBER_1024 must be 2";
}

/**
 * Test: Dilithium variant enum values must not change (ABI stability)
 * Regression: Changing enum values breaks serialization and compatibility
 */
TEST_F(CryptoRegressionV2Test, DilithiumEnumValuesStable)
{
    EXPECT_EQ(NIMCP_PQ_DILITHIUM_2, 0) << "DILITHIUM_2 must be 0";
    EXPECT_EQ(NIMCP_PQ_DILITHIUM_3, 1) << "DILITHIUM_3 must be 1";
    EXPECT_EQ(NIMCP_PQ_DILITHIUM_5, 2) << "DILITHIUM_5 must be 2";
}

/**
 * Test: Kyber parameter constants must not change
 * Regression: Key/ciphertext sizes are part of the specification
 */
TEST_F(CryptoRegressionV2Test, KyberConstantsStable)
{
    // Kyber-512
    EXPECT_EQ(NIMCP_KYBER_512_PUBLIC_KEY_BYTES, 800) << "KYBER_512 public key must be 800";
    EXPECT_EQ(NIMCP_KYBER_512_SECRET_KEY_BYTES, 1632) << "KYBER_512 secret key must be 1632";
    EXPECT_EQ(NIMCP_KYBER_512_CIPHERTEXT_BYTES, 768) << "KYBER_512 ciphertext must be 768";
    EXPECT_EQ(NIMCP_KYBER_512_SHARED_SECRET_BYTES, 32) << "KYBER_512 shared secret must be 32";

    // Kyber-768
    EXPECT_EQ(NIMCP_KYBER_768_PUBLIC_KEY_BYTES, 1184) << "KYBER_768 public key must be 1184";
    EXPECT_EQ(NIMCP_KYBER_768_SECRET_KEY_BYTES, 2400) << "KYBER_768 secret key must be 2400";
    EXPECT_EQ(NIMCP_KYBER_768_CIPHERTEXT_BYTES, 1088) << "KYBER_768 ciphertext must be 1088";
    EXPECT_EQ(NIMCP_KYBER_768_SHARED_SECRET_BYTES, 32) << "KYBER_768 shared secret must be 32";

    // Kyber-1024
    EXPECT_EQ(NIMCP_KYBER_1024_PUBLIC_KEY_BYTES, 1568) << "KYBER_1024 public key must be 1568";
    EXPECT_EQ(NIMCP_KYBER_1024_SECRET_KEY_BYTES, 3168) << "KYBER_1024 secret key must be 3168";
    EXPECT_EQ(NIMCP_KYBER_1024_CIPHERTEXT_BYTES, 1568) << "KYBER_1024 ciphertext must be 1568";
    EXPECT_EQ(NIMCP_KYBER_1024_SHARED_SECRET_BYTES, 32) << "KYBER_1024 shared secret must be 32";
}

/**
 * Test: Dilithium parameter constants must not change
 * Regression: Key/signature sizes are part of the specification
 */
TEST_F(CryptoRegressionV2Test, DilithiumConstantsStable)
{
    // Dilithium-2
    EXPECT_EQ(NIMCP_DILITHIUM_2_PUBLIC_KEY_BYTES, 1312) << "DILITHIUM_2 public key must be 1312";
    EXPECT_EQ(NIMCP_DILITHIUM_2_SECRET_KEY_BYTES, 2528) << "DILITHIUM_2 secret key must be 2528";
    EXPECT_EQ(NIMCP_DILITHIUM_2_SIGNATURE_BYTES, 2420) << "DILITHIUM_2 signature must be 2420";

    // Dilithium-3
    EXPECT_EQ(NIMCP_DILITHIUM_3_PUBLIC_KEY_BYTES, 1952) << "DILITHIUM_3 public key must be 1952";
    EXPECT_EQ(NIMCP_DILITHIUM_3_SECRET_KEY_BYTES, 4000) << "DILITHIUM_3 secret key must be 4000";
    EXPECT_EQ(NIMCP_DILITHIUM_3_SIGNATURE_BYTES, 3293) << "DILITHIUM_3 signature must be 3293";

    // Dilithium-5
    EXPECT_EQ(NIMCP_DILITHIUM_5_PUBLIC_KEY_BYTES, 2592) << "DILITHIUM_5 public key must be 2592";
    EXPECT_EQ(NIMCP_DILITHIUM_5_SECRET_KEY_BYTES, 4864) << "DILITHIUM_5 secret key must be 4864";
    EXPECT_EQ(NIMCP_DILITHIUM_5_SIGNATURE_BYTES, 4595) << "DILITHIUM_5 signature must be 4595";
}

//=============================================================================
// Kyber KEM Backward Compatibility Tests
//=============================================================================

/**
 * Test: Kyber encapsulation/decapsulation round-trip
 * Regression: Must produce identical shared secrets on both sides
 */
TEST_F(CryptoRegressionV2Test, KyberEncapDecapRoundtrip)
{
    nimcp_kyber_keypair_t keypair;
    nimcp_error_t result = nimcp_kyber_keygen(NIMCP_PQ_KYBER_512, &keypair);
    ASSERT_EQ(result, NIMCP_OK) << "keygen must succeed";

    // Encapsulate
    uint8_t ciphertext[NIMCP_KYBER_512_CIPHERTEXT_BYTES];
    uint8_t shared_secret_enc[NIMCP_KYBER_512_SHARED_SECRET_BYTES];
    size_t ct_len = sizeof(ciphertext);

    result = nimcp_kyber_encapsulate(
        NIMCP_PQ_KYBER_512,
        keypair.public_key,
        ciphertext,
        &ct_len,
        shared_secret_enc,
        sizeof(shared_secret_enc)
    );
    EXPECT_EQ(result, NIMCP_OK) << "encapsulate must succeed";
    EXPECT_EQ(ct_len, static_cast<size_t>(NIMCP_KYBER_512_CIPHERTEXT_BYTES))
        << "ciphertext length must match";

    // Decapsulate
    uint8_t shared_secret_dec[NIMCP_KYBER_512_SHARED_SECRET_BYTES];

    result = nimcp_kyber_decapsulate(
        NIMCP_PQ_KYBER_512,
        keypair.secret_key,
        ciphertext,
        ct_len,
        shared_secret_dec,
        sizeof(shared_secret_dec)
    );
    EXPECT_EQ(result, NIMCP_OK) << "decapsulate must succeed";

    // Shared secrets must match
    EXPECT_EQ(memcmp(shared_secret_enc, shared_secret_dec, sizeof(shared_secret_enc)), 0)
        << "Shared secrets must be identical";

    nimcp_kyber_keypair_free(&keypair);
}

/**
 * Test: All Kyber variants work correctly
 * Regression: All security levels must be functional
 */
TEST_F(CryptoRegressionV2Test, KyberAllVariants)
{
    nimcp_kyber_variant_t variants[] = {
        NIMCP_PQ_KYBER_512,
        NIMCP_PQ_KYBER_768,
        NIMCP_PQ_KYBER_1024
    };
    size_t ct_sizes[] = {
        NIMCP_KYBER_512_CIPHERTEXT_BYTES,
        NIMCP_KYBER_768_CIPHERTEXT_BYTES,
        NIMCP_KYBER_1024_CIPHERTEXT_BYTES
    };
    size_t num_variants = sizeof(variants) / sizeof(variants[0]);

    for (size_t i = 0; i < num_variants; i++) {
        nimcp_kyber_keypair_t keypair;
        nimcp_error_t result = nimcp_kyber_keygen(variants[i], &keypair);
        ASSERT_EQ(result, NIMCP_OK) << "keygen must succeed for all variants";

        // Encapsulate/decapsulate
        std::vector<uint8_t> ciphertext(ct_sizes[i]);
        uint8_t shared_secret_enc[32];
        uint8_t shared_secret_dec[32];
        size_t ct_len = ct_sizes[i];

        result = nimcp_kyber_encapsulate(variants[i], keypair.public_key,
                                         ciphertext.data(), &ct_len,
                                         shared_secret_enc, sizeof(shared_secret_enc));
        EXPECT_EQ(result, NIMCP_OK) << "encapsulate must succeed";

        result = nimcp_kyber_decapsulate(variants[i], keypair.secret_key,
                                         ciphertext.data(), ct_len,
                                         shared_secret_dec, sizeof(shared_secret_dec));
        EXPECT_EQ(result, NIMCP_OK) << "decapsulate must succeed";

        EXPECT_EQ(memcmp(shared_secret_enc, shared_secret_dec, 32), 0)
            << "Shared secrets must match";

        nimcp_kyber_keypair_free(&keypair);
    }
}

/**
 * Test: nimcp_kyber_get_sizes() returns correct values
 * Regression: Size queries must match constants
 */
TEST_F(CryptoRegressionV2Test, KyberGetSizes)
{
    size_t pk_len, sk_len, ct_len;
    nimcp_error_t result;

    // Kyber-512
    result = nimcp_kyber_get_sizes(NIMCP_PQ_KYBER_512, &pk_len, &sk_len, &ct_len);
    EXPECT_EQ(result, NIMCP_OK) << "get_sizes must succeed";
    EXPECT_EQ(pk_len, static_cast<size_t>(NIMCP_KYBER_512_PUBLIC_KEY_BYTES)) << "pk_len must match";
    EXPECT_EQ(sk_len, static_cast<size_t>(NIMCP_KYBER_512_SECRET_KEY_BYTES)) << "sk_len must match";
    EXPECT_EQ(ct_len, static_cast<size_t>(NIMCP_KYBER_512_CIPHERTEXT_BYTES)) << "ct_len must match";

    // Kyber-768
    result = nimcp_kyber_get_sizes(NIMCP_PQ_KYBER_768, &pk_len, &sk_len, &ct_len);
    EXPECT_EQ(result, NIMCP_OK) << "get_sizes must succeed";
    EXPECT_EQ(pk_len, static_cast<size_t>(NIMCP_KYBER_768_PUBLIC_KEY_BYTES)) << "pk_len must match";

    // Kyber-1024
    result = nimcp_kyber_get_sizes(NIMCP_PQ_KYBER_1024, &pk_len, &sk_len, &ct_len);
    EXPECT_EQ(result, NIMCP_OK) << "get_sizes must succeed";
    EXPECT_EQ(pk_len, static_cast<size_t>(NIMCP_KYBER_1024_PUBLIC_KEY_BYTES)) << "pk_len must match";
}

//=============================================================================
// Dilithium Signature Backward Compatibility Tests
//=============================================================================

/**
 * Test: Dilithium sign/verify round-trip
 * Regression: Valid signatures must verify successfully
 */
TEST_F(CryptoRegressionV2Test, DilithiumSignVerifyRoundtrip)
{
    nimcp_dilithium_keypair_t keypair;
    nimcp_error_t result = nimcp_dilithium_keygen(NIMCP_PQ_DILITHIUM_2, &keypair);
    ASSERT_EQ(result, NIMCP_OK) << "keygen must succeed";

    const char* message = "Test message for signing";
    uint8_t signature[NIMCP_DILITHIUM_2_SIGNATURE_BYTES];
    size_t sig_len = sizeof(signature);

    // Sign
    result = nimcp_dilithium_sign(
        NIMCP_PQ_DILITHIUM_2,
        keypair.secret_key,
        reinterpret_cast<const uint8_t*>(message),
        strlen(message),
        signature,
        &sig_len
    );
    EXPECT_EQ(result, NIMCP_OK) << "sign must succeed";
    EXPECT_LE(sig_len, static_cast<size_t>(NIMCP_DILITHIUM_2_SIGNATURE_BYTES))
        << "sig_len must be within bounds";

    // Verify
    result = nimcp_dilithium_verify(
        NIMCP_PQ_DILITHIUM_2,
        keypair.public_key,
        reinterpret_cast<const uint8_t*>(message),
        strlen(message),
        signature,
        sig_len
    );
    EXPECT_EQ(result, NIMCP_OK) << "verify must succeed for valid signature";

    nimcp_dilithium_keypair_free(&keypair);
}

/**
 * Test: Tampered message fails verification
 * Regression: Signature verification must detect tampering
 */
TEST_F(CryptoRegressionV2Test, DilithiumTamperDetection)
{
    nimcp_dilithium_keypair_t keypair;
    nimcp_error_t result = nimcp_dilithium_keygen(NIMCP_PQ_DILITHIUM_2, &keypair);
    ASSERT_EQ(result, NIMCP_OK) << "keygen must succeed";

    const char* message = "Original message";
    const char* tampered = "Tampered message";
    uint8_t signature[NIMCP_DILITHIUM_2_SIGNATURE_BYTES];
    size_t sig_len = sizeof(signature);

    // Sign original
    result = nimcp_dilithium_sign(
        NIMCP_PQ_DILITHIUM_2,
        keypair.secret_key,
        reinterpret_cast<const uint8_t*>(message),
        strlen(message),
        signature,
        &sig_len
    );
    EXPECT_EQ(result, NIMCP_OK) << "sign must succeed";

    // Verify tampered - must fail
    result = nimcp_dilithium_verify(
        NIMCP_PQ_DILITHIUM_2,
        keypair.public_key,
        reinterpret_cast<const uint8_t*>(tampered),
        strlen(tampered),
        signature,
        sig_len
    );
    EXPECT_NE(result, NIMCP_OK) << "verify must fail for tampered message";

    nimcp_dilithium_keypair_free(&keypair);
}

/**
 * Test: All Dilithium variants work correctly
 * Regression: All security levels must be functional
 */
TEST_F(CryptoRegressionV2Test, DilithiumAllVariants)
{
    nimcp_dilithium_variant_t variants[] = {
        NIMCP_PQ_DILITHIUM_2,
        NIMCP_PQ_DILITHIUM_3,
        NIMCP_PQ_DILITHIUM_5
    };
    size_t sig_sizes[] = {
        NIMCP_DILITHIUM_2_SIGNATURE_BYTES,
        NIMCP_DILITHIUM_3_SIGNATURE_BYTES,
        NIMCP_DILITHIUM_5_SIGNATURE_BYTES
    };
    size_t num_variants = sizeof(variants) / sizeof(variants[0]);

    const char* message = "Test message for all variants";

    for (size_t i = 0; i < num_variants; i++) {
        nimcp_dilithium_keypair_t keypair;
        nimcp_error_t result = nimcp_dilithium_keygen(variants[i], &keypair);
        ASSERT_EQ(result, NIMCP_OK) << "keygen must succeed for all variants";

        std::vector<uint8_t> signature(sig_sizes[i]);
        size_t sig_len = sig_sizes[i];

        result = nimcp_dilithium_sign(variants[i], keypair.secret_key,
                                      reinterpret_cast<const uint8_t*>(message), strlen(message),
                                      signature.data(), &sig_len);
        EXPECT_EQ(result, NIMCP_OK) << "sign must succeed";

        result = nimcp_dilithium_verify(variants[i], keypair.public_key,
                                        reinterpret_cast<const uint8_t*>(message), strlen(message),
                                        signature.data(), sig_len);
        EXPECT_EQ(result, NIMCP_OK) << "verify must succeed";

        nimcp_dilithium_keypair_free(&keypair);
    }
}

/**
 * Test: nimcp_dilithium_get_sizes() returns correct values
 * Regression: Size queries must match constants
 */
TEST_F(CryptoRegressionV2Test, DilithiumGetSizes)
{
    size_t pk_len, sk_len, sig_len;
    nimcp_error_t result;

    // Dilithium-2
    result = nimcp_dilithium_get_sizes(NIMCP_PQ_DILITHIUM_2, &pk_len, &sk_len, &sig_len);
    EXPECT_EQ(result, NIMCP_OK) << "get_sizes must succeed";
    EXPECT_EQ(pk_len, static_cast<size_t>(NIMCP_DILITHIUM_2_PUBLIC_KEY_BYTES)) << "pk_len must match";
    EXPECT_EQ(sk_len, static_cast<size_t>(NIMCP_DILITHIUM_2_SECRET_KEY_BYTES)) << "sk_len must match";
    EXPECT_EQ(sig_len, static_cast<size_t>(NIMCP_DILITHIUM_2_SIGNATURE_BYTES)) << "sig_len must match";

    // Dilithium-3
    result = nimcp_dilithium_get_sizes(NIMCP_PQ_DILITHIUM_3, &pk_len, &sk_len, &sig_len);
    EXPECT_EQ(result, NIMCP_OK) << "get_sizes must succeed";
    EXPECT_EQ(pk_len, static_cast<size_t>(NIMCP_DILITHIUM_3_PUBLIC_KEY_BYTES)) << "pk_len must match";

    // Dilithium-5
    result = nimcp_dilithium_get_sizes(NIMCP_PQ_DILITHIUM_5, &pk_len, &sk_len, &sig_len);
    EXPECT_EQ(result, NIMCP_OK) << "get_sizes must succeed";
    EXPECT_EQ(pk_len, static_cast<size_t>(NIMCP_DILITHIUM_5_PUBLIC_KEY_BYTES)) << "pk_len must match";
}

//=============================================================================
// Security Level Consistency Tests
//=============================================================================

/**
 * Test: Kyber security levels are correct
 * Regression: Security level mappings must remain accurate
 */
TEST_F(CryptoRegressionV2Test, KyberSecurityLevels)
{
    int level;

    level = nimcp_kyber_security_level(NIMCP_PQ_KYBER_512);
    EXPECT_EQ(level, 128) << "KYBER_512 must be 128-bit security";

    level = nimcp_kyber_security_level(NIMCP_PQ_KYBER_768);
    EXPECT_EQ(level, 192) << "KYBER_768 must be 192-bit security";

    level = nimcp_kyber_security_level(NIMCP_PQ_KYBER_1024);
    EXPECT_EQ(level, 256) << "KYBER_1024 must be 256-bit security";
}

/**
 * Test: Dilithium security levels are correct
 * Regression: Security level mappings must remain accurate
 */
TEST_F(CryptoRegressionV2Test, DilithiumSecurityLevels)
{
    int level;

    level = nimcp_dilithium_security_level(NIMCP_PQ_DILITHIUM_2);
    EXPECT_EQ(level, 128) << "DILITHIUM_2 must be 128-bit security";

    level = nimcp_dilithium_security_level(NIMCP_PQ_DILITHIUM_3);
    EXPECT_EQ(level, 192) << "DILITHIUM_3 must be 192-bit security";

    level = nimcp_dilithium_security_level(NIMCP_PQ_DILITHIUM_5);
    EXPECT_EQ(level, 256) << "DILITHIUM_5 must be 256-bit security";
}

//=============================================================================
// Performance Baseline Tests
//=============================================================================

/**
 * Test: Kyber keygen performance baseline
 * Regression: 100 keygens must complete in reasonable time
 */
TEST_F(CryptoRegressionV2Test, PerformanceKyberKeygen)
{
    const int NUM_ITERATIONS = 100;
    const double MAX_TIME_MS = 10000.0;  // 10 seconds

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        nimcp_kyber_keypair_t keypair;
        nimcp_error_t result = nimcp_kyber_keygen(NIMCP_PQ_KYBER_512, &keypair);
        ASSERT_EQ(result, NIMCP_OK) << "keygen failed during performance test";
        nimcp_kyber_keypair_free(&keypair);
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;

    EXPECT_LT(elapsed.count(), MAX_TIME_MS)
        << "Kyber keygen must meet performance baseline: " << elapsed.count() << " ms";
}

/**
 * Test: Dilithium sign performance baseline
 * Regression: 100 signatures must complete in reasonable time
 */
TEST_F(CryptoRegressionV2Test, PerformanceDilithiumSign)
{
    const int NUM_ITERATIONS = 100;
    const double MAX_TIME_MS = 10000.0;  // 10 seconds

    nimcp_dilithium_keypair_t keypair;
    nimcp_error_t result = nimcp_dilithium_keygen(NIMCP_PQ_DILITHIUM_2, &keypair);
    ASSERT_EQ(result, NIMCP_OK) << "keygen must succeed";

    const char* message = "Performance test message";
    uint8_t signature[NIMCP_DILITHIUM_2_SIGNATURE_BYTES];

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        size_t sig_len = sizeof(signature);
        result = nimcp_dilithium_sign(NIMCP_PQ_DILITHIUM_2, keypair.secret_key,
                                      reinterpret_cast<const uint8_t*>(message), strlen(message),
                                      signature, &sig_len);
        ASSERT_EQ(result, NIMCP_OK) << "sign failed during performance test";
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;

    nimcp_dilithium_keypair_free(&keypair);

    EXPECT_LT(elapsed.count(), MAX_TIME_MS)
        << "Dilithium sign must meet performance baseline: " << elapsed.count() << " ms";
}

//=============================================================================
// Memory Management Tests
//=============================================================================

/**
 * Test: Keypair free handles already-freed safely
 * Regression: Double free protection
 */
TEST_F(CryptoRegressionV2Test, KeypairFreeSafety)
{
    nimcp_kyber_keypair_t kyber_keypair;
    nimcp_error_t result = nimcp_kyber_keygen(NIMCP_PQ_KYBER_512, &kyber_keypair);
    ASSERT_EQ(result, NIMCP_OK) << "keygen must succeed";

    nimcp_kyber_keypair_free(&kyber_keypair);
    // Second free on zeroed struct should be safe
    nimcp_kyber_keypair_free(&kyber_keypair);

    nimcp_dilithium_keypair_t dilithium_keypair;
    result = nimcp_dilithium_keygen(NIMCP_PQ_DILITHIUM_2, &dilithium_keypair);
    ASSERT_EQ(result, NIMCP_OK) << "keygen must succeed";

    nimcp_dilithium_keypair_free(&dilithium_keypair);
    nimcp_dilithium_keypair_free(&dilithium_keypair);

    // NULL should be safe
    nimcp_kyber_keypair_free(nullptr);
    nimcp_dilithium_keypair_free(nullptr);
    SUCCEED();
}

/**
 * Test: Many keygen/free cycles don't leak
 * Regression: Memory leak prevention
 */
TEST_F(CryptoRegressionV2Test, KeygenFreeCycles)
{
    const int NUM_ITERATIONS = 100;

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        nimcp_kyber_keypair_t kyber_keypair;
        nimcp_dilithium_keypair_t dilithium_keypair;

        nimcp_kyber_keygen(NIMCP_PQ_KYBER_768, &kyber_keypair);
        nimcp_dilithium_keygen(NIMCP_PQ_DILITHIUM_3, &dilithium_keypair);

        nimcp_kyber_keypair_free(&kyber_keypair);
        nimcp_dilithium_keypair_free(&dilithium_keypair);
    }

    // If we get here without crashing or running out of memory, success
    SUCCEED();
}

//=============================================================================
// Statistics API Tests
//=============================================================================

/**
 * Test: nimcp_pq_get_stats() contract
 * Regression: Statistics must be retrievable and accurate
 */
TEST_F(CryptoRegressionV2Test, StatisticsApi)
{
    ASSERT_NE(ctx_, nullptr) << "Setup failed";

    nimcp_pq_stats_t stats;
    memset(&stats, 0xFF, sizeof(stats));  // Fill with garbage

    nimcp_error_t result = nimcp_pq_get_stats(ctx_, &stats);
    EXPECT_EQ(result, NIMCP_OK) << "get_stats must succeed";

    // Initial stats should be zero
    EXPECT_EQ(stats.kyber_keygens, 0u) << "Initial kyber_keygens must be 0";
    EXPECT_EQ(stats.dilithium_keygens, 0u) << "Initial dilithium_keygens must be 0";
}

//=============================================================================
// Self-Test Verification
//=============================================================================

/**
 * Test: nimcp_pq_self_test() passes
 * Regression: Self-test must always pass on valid implementation
 */
TEST_F(CryptoRegressionV2Test, SelfTestPasses)
{
    ASSERT_NE(ctx_, nullptr) << "Setup failed";

    nimcp_error_t result = nimcp_pq_self_test(ctx_);
    EXPECT_EQ(result, NIMCP_OK) << "Self-test must pass";
}

}  // anonymous namespace
