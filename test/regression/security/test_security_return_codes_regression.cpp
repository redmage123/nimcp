// test_security_return_codes_regression.cpp - Regression for P2-7 fix
// Ensures security functions never return bare -1, always proper NIMCP error codes
#include <gtest/gtest.h>
#include <cstring>

#include "security/nimcp_security.h"
#include "security/nimcp_post_quantum.h"
#include "security/nimcp_security_consensus.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "security/nimcp_bbb_helpers.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "utils/error/nimcp_error_codes.h"

// Regression macro: verify no bare -1 returns
#define ASSERT_NO_BARE_MINUS1(expr) do { \
    int _v = (int)(expr); \
    ASSERT_NE(_v, -1) << "REGRESSION P2-7: " #expr " returned bare -1"; \
} while(0)

// --- Directive system regression ---
TEST(SecurityReturnCodesRegression, DirectiveAdd_NullSystem) {
    ASSERT_NO_BARE_MINUS1(nimcp_directive_add(nullptr, "test"));
}

TEST(SecurityReturnCodesRegression, DirectiveAdd_NullText) {
    nimcp_directive_system_t* sys = nimcp_directive_system_create();
    if (sys) {
        ASSERT_NO_BARE_MINUS1(nimcp_directive_add(sys, nullptr));
        nimcp_directive_system_destroy(sys);
    }
}

TEST(SecurityReturnCodesRegression, DirectiveLock_NullSystem) {
    ASSERT_NO_BARE_MINUS1(nimcp_directive_lock(nullptr));
}

// --- Encryption regression ---
TEST(SecurityReturnCodesRegression, Encrypt_NullCtx) {
    uint8_t d[] = {1}; uint8_t o[256]; size_t s = 0;
    ASSERT_NO_BARE_MINUS1(nimcp_encryption_encrypt(nullptr, d, 1, o, 256, &s));
}

TEST(SecurityReturnCodesRegression, Decrypt_NullCtx) {
    uint8_t d[] = {1}; uint8_t o[256]; size_t s = 0;
    ASSERT_NO_BARE_MINUS1(nimcp_encryption_decrypt(nullptr, d, 1, o, 256, &s));
}

TEST(SecurityReturnCodesRegression, GenerateKey_NullPtr) {
    ASSERT_NO_BARE_MINUS1(nimcp_encryption_generate_key(nullptr));
}

// --- Sanitization regression ---
TEST(SecurityReturnCodesRegression, SanitizeInput_NullInput) {
    char out[256];
    ASSERT_NO_BARE_MINUS1(nimcp_security_sanitize_input(nullptr, out, 256));
}

TEST(SecurityReturnCodesRegression, SanitizeInput_NullOutput) {
    ASSERT_NO_BARE_MINUS1(nimcp_security_sanitize_input("test", nullptr, 256));
}

TEST(SecurityReturnCodesRegression, SanitizeInput_ZeroSize) {
    char out[256];
    ASSERT_NO_BARE_MINUS1(nimcp_security_sanitize_input("test", out, 0));
}

// --- Post-quantum regression ---
TEST(SecurityReturnCodesRegression, PQGetStats_NullCtx) {
    nimcp_pq_stats_t stats;
    ASSERT_NO_BARE_MINUS1(nimcp_pq_get_stats(nullptr, &stats));
}

TEST(SecurityReturnCodesRegression, PQGetStats_NullStats) {
    nimcp_pq_context_t ctx = nimcp_pq_context_create(nullptr);
    if (ctx) {
        ASSERT_NO_BARE_MINUS1(nimcp_pq_get_stats(ctx, nullptr));
        nimcp_pq_context_destroy(ctx);
    }
}

TEST(SecurityReturnCodesRegression, KyberKeygen_NullKeypair) {
    ASSERT_NO_BARE_MINUS1(nimcp_kyber_keygen(NIMCP_PQ_KYBER_512, nullptr));
}

// --- Consensus regression ---
TEST(SecurityReturnCodesRegression, ConsensusJoin_NullCtx) {
    ASSERT_NO_BARE_MINUS1(nimcp_consensus_join(nullptr, "peer:8080"));
}

TEST(SecurityReturnCodesRegression, ConsensusLeave_NullCtx) {
    ASSERT_NO_BARE_MINUS1(nimcp_consensus_leave(nullptr));
}

TEST(SecurityReturnCodesRegression, ConsensusShareThreat_NullCtx) {
    nimcp_threat_info_t t; memset(&t, 0, sizeof(t));
    ASSERT_NO_BARE_MINUS1(nimcp_consensus_share_threat(nullptr, &t));
}

TEST(SecurityReturnCodesRegression, ConsensusProposePolicy_NullCtx) {
    nimcp_security_policy_t p; memset(&p, 0, sizeof(p));
    ASSERT_NO_BARE_MINUS1(nimcp_consensus_propose_policy(nullptr, &p));
}

TEST(SecurityReturnCodesRegression, ConsensusInitiateResponse_NullCtx) {
    ASSERT_NO_BARE_MINUS1(nimcp_consensus_initiate_response(nullptr, NIMCP_RESPONSE_ISOLATE_NODE, nullptr));
}

// --- Skepticism regression ---
TEST(SecurityReturnCodesRegression, Skepticism_NullResult) {
    ASSERT_NO_BARE_MINUS1(nimcp_security_evaluate_skepticism("claim", "source", "ctx", nullptr));
}

TEST(SecurityReturnCodesRegression, Skepticism_NullClaim) {
    nimcp_skepticism_result_t res;
    ASSERT_NO_BARE_MINUS1(nimcp_security_evaluate_skepticism(nullptr, "source", "ctx", &res));
}

// --- Security log/stats regression ---
TEST(SecurityReturnCodesRegression, LogEvent_NullDetails) {
    ASSERT_NO_BARE_MINUS1(nimcp_security_log_event(NIMCP_SECURITY_EVENT_THREAT_DETECTED, NIMCP_THREAT_LOW, nullptr));
}

TEST(SecurityReturnCodesRegression, GetStats_AllNull) {
    ASSERT_NO_BARE_MINUS1(nimcp_security_get_stats(nullptr, nullptr, nullptr));
}

// --- Immune system regression ---
TEST(SecurityReturnCodesRegression, ImmuneCreate_NullConfig) {
    brain_immune_system_t* sys = brain_immune_create(nullptr);
    if (sys) brain_immune_destroy(sys);
}

TEST(SecurityReturnCodesRegression, ImmunePresentBBBThreat_NullSystem) {
    uint32_t aid = 0;
    uint8_t data[] = {1};
    int rc = brain_immune_present_bbb_threat(nullptr, BBB_THREAT_BUFFER_OVERFLOW, BBB_SEVERITY_HIGH, data, 1, &aid);
    ASSERT_NE(rc, -1) << "REGRESSION P2-7: bare -1 returned";
}

TEST(SecurityReturnCodesRegression, ImmuneGetStats_NullSystem) {
    brain_immune_stats_t stats;
    int rc = brain_immune_get_stats(nullptr, &stats);
    ASSERT_NE(rc, -1) << "REGRESSION P2-7: bare -1 returned";
}

TEST(SecurityReturnCodesRegression, ImmuneUpdate_NullSystem) {
    int rc = brain_immune_update(nullptr, 100);
    ASSERT_NE(rc, -1) << "REGRESSION P2-7: bare -1 returned";
}

TEST(SecurityReturnCodesRegression, ImmuneConnectBBB_NullSystem) {
    bbb_system_t bbb = bbb_system_create(nullptr);
    if (bbb) {
        int rc = brain_immune_connect_bbb(nullptr, bbb);
        ASSERT_NE(rc, -1) << "REGRESSION P2-7: bare -1 returned";
        bbb_system_destroy(bbb);
    }
}

// --- Error code value regression ---
TEST(SecurityReturnCodesRegression, ErrorCodesInRange) {
    EXPECT_EQ(NIMCP_ERROR_SECURITY_BASE, 9000);
    EXPECT_GT((int)NIMCP_ERROR_BBB_REJECTED, (int)NIMCP_ERROR_SECURITY_BASE);
    EXPECT_GT((int)NIMCP_ERROR_BBB_VALIDATION, (int)NIMCP_ERROR_SECURITY_BASE);
    EXPECT_GT((int)NIMCP_ERROR_SECURITY_THREAT, (int)NIMCP_ERROR_SECURITY_BASE);
    EXPECT_GE((int)NIMCP_ERROR_NULL_POINTER, 1000);
    EXPECT_GE((int)NIMCP_ERROR_INVALID_STATE, 1000);
}
