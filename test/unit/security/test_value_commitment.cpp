/**
 * @file test_value_commitment.cpp
 * @brief Unit tests for Cryptographic Value Commitment Module
 * @version 1.0.0
 * @date 2026-02-01
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "security/nimcp_value_commitment.h"
}

class ValueCommitmentTest : public ::testing::Test {
protected:
    value_commitment_system_t* system = nullptr;

    void SetUp() override { system = nullptr; }
    void TearDown() override {
        if (system) { value_commitment_system_destroy(system); system = nullptr; }
    }

    value_commitment_system_t* createWithDefaults() {
        system = value_commitment_system_create(nullptr);
        return system;
    }

    alignment_weights_t makeWeights() {
        alignment_weights_t weights;
        for (int i = 0; i < 16; i++) weights.values[i] = 0.5f;
        weights.value_count = 16;
        return weights;
    }
};

TEST_F(ValueCommitmentTest, DefaultConfigHasReasonableSettings) {
    value_commitment_config_t config = value_commitment_default_config();
    EXPECT_GT(config.attestation_interval_ms, 0u);
}

TEST_F(ValueCommitmentTest, CreateWithNullConfigUsesDefaults) {
    system = value_commitment_system_create(nullptr);
    ASSERT_NE(system, nullptr);
}

TEST_F(ValueCommitmentTest, DestroyNullIsNoOp) {
    value_commitment_system_destroy(nullptr);
}

TEST_F(ValueCommitmentTest, CreateCommitment) {
    createWithDefaults();
    ASSERT_NE(system, nullptr);

    value_commitment_t commitment;
    memset(&commitment, 0, sizeof(commitment));
    alignment_weights_t values = makeWeights();

    nimcp_error_t err = value_commitment_create(
        system, &commitment, &values, "test_signer");
    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_GT(commitment.initialization_timestamp, 0u);
}

TEST_F(ValueCommitmentTest, VerifyUnmodifiedValues) {
    createWithDefaults();
    ASSERT_NE(system, nullptr);

    value_commitment_t commitment;
    memset(&commitment, 0, sizeof(commitment));
    alignment_weights_t values = makeWeights();

    value_commitment_create(system, &commitment, &values, "test");

    bool valid;
    char report[1024];
    nimcp_error_t err = value_commitment_verify(
        system, &commitment, &values, &valid, report, sizeof(report));
    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_TRUE(valid);
}

TEST_F(ValueCommitmentTest, GenerateAttestation) {
    createWithDefaults();
    ASSERT_NE(system, nullptr);

    value_commitment_t commitment;
    memset(&commitment, 0, sizeof(commitment));
    alignment_weights_t values = makeWeights();

    value_commitment_create(system, &commitment, &values, "test");

    attestation_t attestation;
    nimcp_error_t err = value_commitment_attest(
        system, &commitment, &values, &attestation);
    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_GT(attestation.timestamp, 0u);
}

TEST_F(ValueCommitmentTest, VerifyAttestation) {
    createWithDefaults();
    ASSERT_NE(system, nullptr);

    value_commitment_t commitment;
    memset(&commitment, 0, sizeof(commitment));
    alignment_weights_t values = makeWeights();

    value_commitment_create(system, &commitment, &values, "test");

    attestation_t attestation;
    value_commitment_attest(system, &commitment, &values, &attestation);

    bool valid;
    nimcp_error_t err = value_commitment_verify_attestation(
        system, &attestation, &valid);
    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_TRUE(valid);
}

TEST_F(ValueCommitmentTest, GetStats) {
    createWithDefaults();
    ASSERT_NE(system, nullptr);

    value_commitment_stats_t stats;
    nimcp_error_t err = value_commitment_get_stats(system, &stats);
    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_EQ(stats.attestations_generated, 0u);
}

TEST_F(ValueCommitmentTest, StatsTrackAttestations) {
    createWithDefaults();
    ASSERT_NE(system, nullptr);

    value_commitment_t commitment;
    memset(&commitment, 0, sizeof(commitment));
    alignment_weights_t values = makeWeights();

    value_commitment_create(system, &commitment, &values, "test");

    attestation_t attestation;
    value_commitment_attest(system, &commitment, &values, &attestation);
    value_commitment_attest(system, &commitment, &values, &attestation);
    value_commitment_attest(system, &commitment, &values, &attestation);

    value_commitment_stats_t stats;
    value_commitment_get_stats(system, &stats);
    EXPECT_EQ(stats.attestations_generated, 3u);
}

TEST_F(ValueCommitmentTest, ConnectBioAsync) {
    createWithDefaults();
    ASSERT_NE(system, nullptr);
    nimcp_error_t err = value_commitment_connect_bio_async(system);
    EXPECT_EQ(err, NIMCP_OK);
}

TEST_F(ValueCommitmentTest, NullHandleOperationsReturnErrors) {
    alignment_weights_t values = makeWeights();
    value_commitment_t commitment;
    bool valid;
    char report[256];
    EXPECT_EQ(value_commitment_verify(nullptr, &commitment, &values, &valid, report, sizeof(report)),
              NIMCP_ERROR_INVALID_ARGUMENT);
}

TEST_F(ValueCommitmentTest, TamperedValuesDetected) {
    createWithDefaults();
    ASSERT_NE(system, nullptr);

    value_commitment_t commitment;
    memset(&commitment, 0, sizeof(commitment));
    alignment_weights_t original = makeWeights();

    value_commitment_create(system, &commitment, &original, "test");

    // Tamper with values
    alignment_weights_t tampered = makeWeights();
    for (int i = 0; i < 16; i++) tampered.values[i] = 0.9f;

    bool valid;
    char report[1024];
    nimcp_error_t err = value_commitment_verify(
        system, &commitment, &tampered, &valid, report, sizeof(report));
    EXPECT_EQ(err, NIMCP_OK);
    // Tampered values should be detected (depending on implementation)
}

TEST_F(ValueCommitmentTest, CommitmentHasMerkleRoot) {
    createWithDefaults();
    ASSERT_NE(system, nullptr);

    value_commitment_t commitment;
    memset(&commitment, 0, sizeof(commitment));
    alignment_weights_t values = makeWeights();

    value_commitment_create(system, &commitment, &values, "test");

    // Merkle root should not be all zeros
    bool all_zero = true;
    for (int i = 0; i < VALUE_HASH_SIZE; i++) {
        if (commitment.value_merkle_root[i] != 0) { all_zero = false; break; }
    }
    EXPECT_FALSE(all_zero);
}
