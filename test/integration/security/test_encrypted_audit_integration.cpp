/**
 * @file test_encrypted_audit_integration.cpp
 * @brief Integration tests for Encrypted Audit with Security Systems
 *
 * WHAT: Tests encrypted audit integration with NIMCP components
 * WHY:  Verify end-to-end audit logging workflows
 * HOW:  Test real security events and integration points
 *
 * @author NIMCP Security Team
 * @date 2025-12-07
 */

#include <gtest/gtest.h>
#include "security/nimcp_encrypted_audit.h"
#include "security/nimcp_pattern_db.h"
#include "security/nimcp_security.h"

class EncryptedAuditIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_encryption_generate_key(test_key);
        config = nimcp_encrypted_audit_default_config();
        audit = nimcp_encrypted_audit_create(&config, test_key, sizeof(test_key));
        ASSERT_NE(audit, nullptr);
    }

    void TearDown() override {
        if (audit) {
            nimcp_encrypted_audit_destroy(audit);
        }
    }

    uint8_t test_key[NIMCP_AUDIT_KEY_SIZE];
    nimcp_encrypted_audit_config_t config;
    nimcp_encrypted_audit_t audit;
};

TEST_F(EncryptedAuditIntegrationTest, AuditSecurityEvent) {
    // Simulate security event detection
    const char* attack = "<script>alert('XSS')</script>";

    EXPECT_EQ(nimcp_encrypted_audit_log(
        audit,
        NIMCP_AUDIT_CRITICAL,
        NIMCP_AUDIT_THREAT,
        "XSS attack detected",
        attack,
        strlen(attack)
    ), NIMCP_SUCCESS);

    // Verify audit was logged
    nimcp_audit_entry_t entries[10];
    size_t num_entries = 0;
    EXPECT_EQ(nimcp_encrypted_audit_read(audit, entries, 10, &num_entries), NIMCP_SUCCESS);
    EXPECT_EQ(num_entries, 1);
    EXPECT_EQ(entries[0].severity, NIMCP_AUDIT_CRITICAL);
    EXPECT_EQ(entries[0].category, NIMCP_AUDIT_THREAT);
}

TEST_F(EncryptedAuditIntegrationTest, AuditPatternDatabaseUpdates) {
    EXPECT_EQ(nimcp_encrypted_audit_log(
        audit,
        NIMCP_AUDIT_INFO,
        NIMCP_AUDIT_PATTERN,
        "Pattern database updated",
        nullptr,
        0
    ), NIMCP_SUCCESS);

    nimcp_encrypted_audit_stats_t stats;
    nimcp_encrypted_audit_get_stats(audit, &stats);
    EXPECT_EQ(stats.total_entries, 1);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
