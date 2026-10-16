/**
 * @file test_capability_control.cpp
 * @brief Unit tests for Capability Control Module
 * @version 1.0.0
 * @date 2026-02-01
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "security/nimcp_capability_control.h"
}

class CapabilityControlTest : public ::testing::Test {
protected:
    capability_control_t* control = nullptr;

    void SetUp() override { control = nullptr; }
    void TearDown() override {
        if (control) { capability_control_destroy(control); control = nullptr; }
    }

    capability_control_t* createWithDefaults() {
        control = capability_control_create(nullptr);
        return control;
    }
};

TEST_F(CapabilityControlTest, DefaultConfigHasRestrictiveSettings) {
    capability_control_config_t config = capability_control_default_config();
    // Self-modification must be disabled
    EXPECT_FALSE(config.envelope.self_mod.can_modify_own_code);
    EXPECT_FALSE(config.envelope.self_mod.can_modify_own_weights);
    EXPECT_FALSE(config.envelope.self_mod.can_modify_safety_systems);
    // Data exfiltration must be disabled
    EXPECT_FALSE(config.envelope.information.can_exfiltrate_data);
}

TEST_F(CapabilityControlTest, SafeEnvelopeIsFullyRestricted) {
    capability_envelope_t envelope = capability_envelope_safe();
    EXPECT_FALSE(envelope.self_mod.can_modify_own_code);
    EXPECT_FALSE(envelope.self_mod.can_modify_own_weights);
    EXPECT_FALSE(envelope.self_mod.can_modify_safety_systems);
    EXPECT_FALSE(envelope.information.can_exfiltrate_data);
    EXPECT_FALSE(envelope.physical.can_control_actuators);
}

TEST_F(CapabilityControlTest, CreateWithNullConfigUsesDefaults) {
    control = capability_control_create(nullptr);
    ASSERT_NE(control, nullptr);
}

TEST_F(CapabilityControlTest, DestroyNullIsNoOp) {
    capability_control_destroy(nullptr);
}

TEST_F(CapabilityControlTest, CheckActionWithinEnvelope) {
    createWithDefaults();
    ASSERT_NE(control, nullptr);

    capability_action_t action;
    memset(&action, 0, sizeof(action));
    strcpy(action.action_type, "compute");
    action.category = CAPABILITY_RESOURCE;
    action.memory_required = 1024;
    action.compute_required = 1000;

    capability_check_result_t result;
    nimcp_error_t err = capability_control_check_action(control, &action, &result);
    EXPECT_EQ(err, NIMCP_OK);
    // Resource-only action within limits should be allowed
}

TEST_F(CapabilityControlTest, CheckSelfModificationBlocked) {
    createWithDefaults();
    ASSERT_NE(control, nullptr);

    capability_action_t action;
    memset(&action, 0, sizeof(action));
    strcpy(action.action_type, "modify_code");
    action.category = CAPABILITY_SELF_MODIFICATION;
    action.modifies_self = true;

    capability_check_result_t result;
    nimcp_error_t err = capability_control_check_action(control, &action, &result);
    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_FALSE(result.allowed);
    EXPECT_EQ(result.violated_category, CAPABILITY_SELF_MODIFICATION);
}

TEST_F(CapabilityControlTest, CheckNetworkAccess) {
    createWithDefaults();
    ASSERT_NE(control, nullptr);

    bool allowed;
    char denial_reason[512];
    nimcp_error_t err = capability_control_check_network(
        control, "api.example.com", true, false, &allowed, denial_reason, sizeof(denial_reason));
    EXPECT_EQ(err, NIMCP_OK);
}

TEST_F(CapabilityControlTest, CheckResourcesWithinLimits) {
    createWithDefaults();
    ASSERT_NE(control, nullptr);

    bool allowed;
    nimcp_error_t err = capability_control_check_resources(
        control, 1024 * 1024, 1000000, &allowed);
    EXPECT_EQ(err, NIMCP_OK);
}

TEST_F(CapabilityControlTest, GetUsage) {
    createWithDefaults();
    ASSERT_NE(control, nullptr);

    capability_resource_usage_t usage;
    nimcp_error_t err = capability_control_get_usage(control, &usage);
    EXPECT_EQ(err, NIMCP_OK);
}

TEST_F(CapabilityControlTest, GetStats) {
    createWithDefaults();
    ASSERT_NE(control, nullptr);

    capability_control_stats_t stats;
    nimcp_error_t err = capability_control_get_stats(control, &stats);
    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_EQ(stats.total_actions_checked, 0u);
}

TEST_F(CapabilityControlTest, StatsTrackChecks) {
    createWithDefaults();
    ASSERT_NE(control, nullptr);

    capability_action_t action;
    memset(&action, 0, sizeof(action));
    strcpy(action.action_type, "compute");
    action.category = CAPABILITY_RESOURCE;

    capability_check_result_t result;
    capability_control_check_action(control, &action, &result);
    capability_control_check_action(control, &action, &result);

    capability_control_stats_t stats;
    capability_control_get_stats(control, &stats);
    EXPECT_EQ(stats.total_actions_checked, 2u);
}

TEST_F(CapabilityControlTest, GetEnvelope) {
    createWithDefaults();
    ASSERT_NE(control, nullptr);

    capability_envelope_t envelope;
    nimcp_error_t err = capability_control_get_envelope(control, &envelope);
    EXPECT_EQ(err, NIMCP_OK);
}

TEST_F(CapabilityControlTest, CategoryNames) {
    EXPECT_NE(capability_category_name(CAPABILITY_NETWORK), nullptr);
    EXPECT_NE(capability_category_name(CAPABILITY_SELF_MODIFICATION), nullptr);
    EXPECT_NE(capability_category_name(CAPABILITY_RESOURCE), nullptr);
    EXPECT_NE(capability_category_name(CAPABILITY_INFORMATION), nullptr);
    EXPECT_NE(capability_category_name(CAPABILITY_PHYSICAL), nullptr);
}

TEST_F(CapabilityControlTest, DomainMatching) {
    EXPECT_TRUE(capability_domain_matches("api.example.com", "api.example.com"));
    EXPECT_TRUE(capability_domain_matches("api.example.com", "*.example.com"));
    EXPECT_FALSE(capability_domain_matches("api.example.com", "other.com"));
}

TEST_F(CapabilityControlTest, ConnectBioAsync) {
    createWithDefaults();
    ASSERT_NE(control, nullptr);
    nimcp_error_t err = capability_control_connect_bio_async(control);
    EXPECT_EQ(err, NIMCP_OK);
}

TEST_F(CapabilityControlTest, NullHandleOperationsReturnErrors) {
    capability_action_t action;
    memset(&action, 0, sizeof(action));
    capability_check_result_t result;
    EXPECT_EQ(capability_control_check_action(nullptr, &action, &result),
              NIMCP_ERROR_INVALID_ARGUMENT);
}

