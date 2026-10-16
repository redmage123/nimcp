// test_security_error_codes.cpp - Security functions return proper NIMCP error codes
// Tests P2-7: No bare -1 returns; all return nimcp_error_t codes
#include <gtest/gtest.h>
#include <cstring>

#include "security/nimcp_security.h"
#include "security/nimcp_post_quantum.h"
#include "security/nimcp_security_consensus.h"
#include "utils/error/nimcp_error_codes.h"

// Helper: verify return is a valid NIMCP error code (not bare -1)
#define EXPECT_VALID_NIMCP(expr) do { \
    auto _r = (int)(expr); \
    EXPECT_NE(_r, -1) << #expr " returned bare -1 (P2-7 violation)"; \
    if (_r != 0) { EXPECT_GE(_r, (int)NIMCP_ERROR_UNKNOWN) << #expr " returned non-standard error " << _r; } \
} while(0)

// --- Directive System ---
TEST(SecErrCode, DirectiveAdd_NullSystem) {
    EXPECT_VALID_NIMCP(nimcp_directive_add(nullptr, "test"));
}

TEST(SecErrCode, DirectiveAdd_NullText) {
    nimcp_directive_system_t* sys = nimcp_directive_system_create();
    if (sys) {
        EXPECT_VALID_NIMCP(nimcp_directive_add(sys, nullptr));
        nimcp_directive_system_destroy(sys);
    }
}

TEST(SecErrCode, DirectiveLock_NullSystem) {
    EXPECT_VALID_NIMCP(nimcp_directive_lock(nullptr));
}

// --- Encryption ---
TEST(SecErrCode, EncryptNull) {
    uint8_t d[] = {1, 2, 3}; uint8_t o[256]; size_t s = 0;
    EXPECT_VALID_NIMCP(nimcp_encryption_encrypt(nullptr, d, 3, o, 256, &s));
}

TEST(SecErrCode, DecryptNull) {
    uint8_t d[] = {1, 2, 3}; uint8_t o[256]; size_t s = 0;
    EXPECT_VALID_NIMCP(nimcp_encryption_decrypt(nullptr, d, 3, o, 256, &s));
}

TEST(SecErrCode, GenKeyNull) {
    EXPECT_VALID_NIMCP(nimcp_encryption_generate_key(nullptr));
}

// --- Input Sanitization ---
TEST(SecErrCode, SanitizeInput_NullInput) {
    char out[256];
    EXPECT_VALID_NIMCP(nimcp_security_sanitize_input(nullptr, out, 256));
}

TEST(SecErrCode, SanitizeInput_NullOutput) {
    EXPECT_VALID_NIMCP(nimcp_security_sanitize_input("test", nullptr, 256));
}

TEST(SecErrCode, SanitizeInput_ZeroSize) {
    char out[256];
    EXPECT_VALID_NIMCP(nimcp_security_sanitize_input("test", out, 0));
}

TEST(SecErrCode, SanitizeInput_Valid) {
    char out[256];
    nimcp_result_t r = nimcp_security_sanitize_input("safe input", out, 256);
    EXPECT_EQ(r, NIMCP_OK);
}

// --- Skepticism ---
TEST(SecErrCode, Skepticism_NullResult) {
    EXPECT_VALID_NIMCP(nimcp_security_evaluate_skepticism("claim", "source", "context", nullptr));
}

TEST(SecErrCode, Skepticism_NullClaim) {
    nimcp_skepticism_result_t res;
    EXPECT_VALID_NIMCP(nimcp_security_evaluate_skepticism(nullptr, "source", "context", &res));
}

// --- Security Audit/Stats ---
TEST(SecErrCode, LogEvent_Valid) {
    EXPECT_VALID_NIMCP(nimcp_security_log_event(NIMCP_SECURITY_EVENT_DIRECTIVE_VERIFIED, NIMCP_THREAT_LOW, "test event"));
}

TEST(SecErrCode, LogEvent_NullDetails) {
    EXPECT_VALID_NIMCP(nimcp_security_log_event(NIMCP_SECURITY_EVENT_DIRECTIVE_VERIFIED, NIMCP_THREAT_LOW, nullptr));
}

TEST(SecErrCode, GetStats_NullArgs) {
    EXPECT_VALID_NIMCP(nimcp_security_get_stats(nullptr, nullptr, nullptr));
}

TEST(SecErrCode, GetStats_Valid) {
    uint64_t threats = 0, rejected = 0, verified = 0;
    nimcp_result_t r = nimcp_security_get_stats(&threats, &rejected, &verified);
    EXPECT_VALID_NIMCP(r);
}

// --- Post-Quantum ---
TEST(SecErrCode, PQGetStats_NullCtx) {
    nimcp_pq_stats_t stats;
    EXPECT_VALID_NIMCP(nimcp_pq_get_stats(nullptr, &stats));
}

TEST(SecErrCode, PQGetStats_NullStats) {
    nimcp_pq_context_t ctx = nimcp_pq_context_create(nullptr);
    if (ctx) {
        EXPECT_VALID_NIMCP(nimcp_pq_get_stats(ctx, nullptr));
        nimcp_pq_context_destroy(ctx);
    }
}

TEST(SecErrCode, KyberKeygen_NullKeypair) {
    EXPECT_VALID_NIMCP(nimcp_kyber_keygen(NIMCP_PQ_KYBER_512, nullptr));
}

// --- Consensus ---
TEST(SecErrCode, ConsensusJoin_NullCtx) {
    EXPECT_VALID_NIMCP(nimcp_consensus_join(nullptr, "peer:8080"));
}

TEST(SecErrCode, ConsensusJoin_NullAddr) {
    nimcp_consensus_config_t cfg; memset(&cfg, 0, sizeof(cfg));
    nimcp_security_consensus_t ctx = nimcp_consensus_create(&cfg);
    if (ctx) {
        EXPECT_VALID_NIMCP(nimcp_consensus_join(ctx, nullptr));
        nimcp_consensus_destroy(ctx);
    }
}

TEST(SecErrCode, ConsensusLeave_NullCtx) {
    EXPECT_VALID_NIMCP(nimcp_consensus_leave(nullptr));
}

TEST(SecErrCode, ConsensusShareThreat_NullCtx) {
    nimcp_threat_info_t t; memset(&t, 0, sizeof(t));
    EXPECT_VALID_NIMCP(nimcp_consensus_share_threat(nullptr, &t));
}

TEST(SecErrCode, ConsensusShareThreat_NullThreat) {
    nimcp_consensus_config_t cfg; memset(&cfg, 0, sizeof(cfg));
    nimcp_security_consensus_t ctx = nimcp_consensus_create(&cfg);
    if (ctx) {
        EXPECT_VALID_NIMCP(nimcp_consensus_share_threat(ctx, nullptr));
        nimcp_consensus_destroy(ctx);
    }
}

TEST(SecErrCode, ConsensusProposePolicy_NullCtx) {
    nimcp_security_policy_t p; memset(&p, 0, sizeof(p));
    EXPECT_VALID_NIMCP(nimcp_consensus_propose_policy(nullptr, &p));
}

TEST(SecErrCode, ConsensusInitiateResponse_NullCtx) {
    EXPECT_VALID_NIMCP(nimcp_consensus_initiate_response(nullptr, NIMCP_RESPONSE_ISOLATE_NODE, nullptr));
}

// --- Verify error code constants ---
TEST(SecErrCode, SuccessIsZero) {
    EXPECT_EQ(NIMCP_SUCCESS, 0);
    EXPECT_EQ(NIMCP_OK, 0);
}

TEST(SecErrCode, SecurityErrorRange) {
    EXPECT_GE(NIMCP_ERROR_BBB_REJECTED, 9000);
    EXPECT_GE(NIMCP_ERROR_BBB_VALIDATION, 9000);
    EXPECT_GE(NIMCP_ERROR_SECURITY_THREAT, 9000);
}
