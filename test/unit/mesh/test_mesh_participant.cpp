/**
 * @file test_mesh_participant.cpp
 * @brief Unit tests for mesh participant and registry
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_transaction.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class MeshParticipantTest : public ::testing::Test {
protected:
    mesh_participant_registry_t* registry = nullptr;

    void SetUp() override {
        mesh_registry_config_t config;
        mesh_registry_default_config(&config);
        config.enable_logging = false;
        registry = mesh_registry_create(&config);
        ASSERT_NE(registry, nullptr);
    }

    void TearDown() override {
        mesh_registry_destroy(registry);
        registry = nullptr;
    }
};

/* ============================================================================
 * Registry Lifecycle Tests
 * ============================================================================ */

TEST_F(MeshParticipantTest, CreateRegistry) {
    EXPECT_NE(registry, nullptr);
}

TEST(MeshParticipantBasicTest, CreateRegistryWithDefaults) {
    mesh_participant_registry_t* reg = mesh_registry_create(NULL);
    EXPECT_NE(reg, nullptr);
    mesh_registry_destroy(reg);
}

TEST(MeshParticipantBasicTest, DestroyNullRegistry) {
    /* Should not crash */
    mesh_registry_destroy(NULL);
}

TEST(MeshParticipantBasicTest, DefaultConfig) {
    mesh_registry_config_t config;
    nimcp_error_t err = mesh_registry_default_config(&config);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GT(config.initial_capacity, 0);
}

TEST(MeshParticipantBasicTest, DefaultConfigNullPtr) {
    nimcp_error_t err = mesh_registry_default_config(NULL);
    EXPECT_EQ(err, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Registration Tests
 * ============================================================================ */

TEST_F(MeshParticipantTest, RegisterParticipant) {
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);

    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "test_module";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = MESH_CHANNEL_LEFT_HEMISPHERE;

    mesh_participant_id_t id;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &id);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_NE(id, 0);
    EXPECT_EQ(mesh_get_channel(id), MESH_CHANNEL_LEFT_HEMISPHERE);
    EXPECT_EQ(mesh_get_participant_type(id), MESH_PARTICIPANT_MODULE);
}

TEST_F(MeshParticipantTest, RegisterMultipleParticipants) {
    mesh_participant_id_t ids[5];

    for (int i = 0; i < 5; i++) {
        mesh_participant_interface_t iface;
        mesh_participant_interface_init(&iface);

        mesh_participant_config_t config;
        mesh_participant_config_init(&config);
        char name[32];
        snprintf(name, sizeof(name), "module_%d", i);
        config.module_name = name;
        config.type = MESH_PARTICIPANT_MODULE;
        config.home_channel = (mesh_channel_id_t)(i % MESH_MAX_CHANNELS);

        nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &ids[i]);
        EXPECT_EQ(err, NIMCP_SUCCESS);
    }

    /* All IDs should be unique */
    for (int i = 0; i < 5; i++) {
        for (int j = i + 1; j < 5; j++) {
            EXPECT_NE(ids[i], ids[j]);
        }
    }
}

TEST_F(MeshParticipantTest, RegisterDuplicateName) {
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);

    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "duplicate_test";
    config.type = MESH_PARTICIPANT_MODULE;

    mesh_participant_id_t id1, id2;
    EXPECT_EQ(mesh_participant_register(registry, &iface, &config, &id1), NIMCP_SUCCESS);
    EXPECT_EQ(mesh_participant_register(registry, &iface, &config, &id2), NIMCP_ERROR_ALREADY_EXISTS);
}

TEST_F(MeshParticipantTest, RegisterNullParams) {
    mesh_participant_interface_t iface;
    mesh_participant_config_t config;
    mesh_participant_id_t id;

    EXPECT_EQ(mesh_participant_register(NULL, &iface, &config, &id), NIMCP_ERROR_INVALID_PARAM);
    EXPECT_EQ(mesh_participant_register(registry, NULL, &config, &id), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(mesh_participant_register(registry, &iface, NULL, &id), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(mesh_participant_register(registry, &iface, &config, NULL), NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Unregistration Tests
 * ============================================================================ */

TEST_F(MeshParticipantTest, UnregisterParticipant) {
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);

    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "unregister_test";

    mesh_participant_id_t id;
    EXPECT_EQ(mesh_participant_register(registry, &iface, &config, &id), NIMCP_SUCCESS);
    EXPECT_TRUE(mesh_participant_is_registered(registry, id));

    EXPECT_EQ(mesh_participant_unregister(registry, id), NIMCP_SUCCESS);
    EXPECT_FALSE(mesh_participant_is_registered(registry, id));
}

TEST_F(MeshParticipantTest, UnregisterNotFound) {
    mesh_participant_id_t fake_id = 0xDEADBEEFDEADBEEF;
    EXPECT_EQ(mesh_participant_unregister(registry, fake_id), NIMCP_ERROR_NOT_FOUND);
}

/* ============================================================================
 * Lookup Tests
 * ============================================================================ */

TEST_F(MeshParticipantTest, GetById) {
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "lookup_test", MESH_MAX_NAME_LEN);

    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "lookup_test";

    mesh_participant_id_t id;
    EXPECT_EQ(mesh_participant_register(registry, &iface, &config, &id), NIMCP_SUCCESS);

    const mesh_participant_interface_t* found = mesh_participant_get(registry, id);
    EXPECT_NE(found, nullptr);
    EXPECT_STREQ(found->module_name, "lookup_test");
}

TEST_F(MeshParticipantTest, GetByName) {
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);

    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "named_module";

    mesh_participant_id_t id;
    EXPECT_EQ(mesh_participant_register(registry, &iface, &config, &id), NIMCP_SUCCESS);

    const mesh_participant_interface_t* found = mesh_participant_get_by_name(registry, "named_module");
    EXPECT_NE(found, nullptr);
    EXPECT_EQ(found->id, id);
}

TEST_F(MeshParticipantTest, GetNotFound) {
    EXPECT_EQ(mesh_participant_get(registry, 0xFFFFFFFF), nullptr);
    EXPECT_EQ(mesh_participant_get_by_name(registry, "nonexistent"), nullptr);
}

/* ============================================================================
 * Channel Membership Tests
 * ============================================================================ */

TEST_F(MeshParticipantTest, JoinChannel) {
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);

    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "channel_test";
    config.home_channel = MESH_CHANNEL_LEFT_HEMISPHERE;

    mesh_participant_id_t id;
    EXPECT_EQ(mesh_participant_register(registry, &iface, &config, &id), NIMCP_SUCCESS);

    /* Should be in home channel */
    EXPECT_TRUE(mesh_participant_is_in_channel(registry, id, MESH_CHANNEL_LEFT_HEMISPHERE));

    /* Join another channel */
    EXPECT_EQ(mesh_participant_join_channel(registry, id, MESH_CHANNEL_RIGHT_HEMISPHERE), NIMCP_SUCCESS);
    EXPECT_TRUE(mesh_participant_is_in_channel(registry, id, MESH_CHANNEL_RIGHT_HEMISPHERE));
}

TEST_F(MeshParticipantTest, LeaveChannel) {
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);

    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "leave_test";
    config.home_channel = MESH_CHANNEL_LEFT_HEMISPHERE;

    mesh_participant_id_t id;
    EXPECT_EQ(mesh_participant_register(registry, &iface, &config, &id), NIMCP_SUCCESS);

    /* Join and leave */
    EXPECT_EQ(mesh_participant_join_channel(registry, id, MESH_CHANNEL_SUBCORTICAL), NIMCP_SUCCESS);
    EXPECT_TRUE(mesh_participant_is_in_channel(registry, id, MESH_CHANNEL_SUBCORTICAL));

    EXPECT_EQ(mesh_participant_leave_channel(registry, id, MESH_CHANNEL_SUBCORTICAL), NIMCP_SUCCESS);
    EXPECT_FALSE(mesh_participant_is_in_channel(registry, id, MESH_CHANNEL_SUBCORTICAL));
}

TEST_F(MeshParticipantTest, CannotLeaveHomeChannel) {
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);

    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "home_test";
    config.home_channel = MESH_CHANNEL_LEFT_HEMISPHERE;

    mesh_participant_id_t id;
    EXPECT_EQ(mesh_participant_register(registry, &iface, &config, &id), NIMCP_SUCCESS);

    EXPECT_EQ(mesh_participant_leave_channel(registry, id, MESH_CHANNEL_LEFT_HEMISPHERE),
              NIMCP_ERROR_PERMISSION_DENIED);
}

TEST_F(MeshParticipantTest, GetChannelMembers) {
    /* Register 3 participants in same channel */
    mesh_participant_id_t ids[3];
    for (int i = 0; i < 3; i++) {
        mesh_participant_interface_t iface;
        mesh_participant_interface_init(&iface);

        mesh_participant_config_t config;
        mesh_participant_config_init(&config);
        char name[32];
        snprintf(name, sizeof(name), "member_%d", i);
        config.module_name = name;
        config.home_channel = MESH_CHANNEL_SUBCORTICAL;

        EXPECT_EQ(mesh_participant_register(registry, &iface, &config, &ids[i]), NIMCP_SUCCESS);
    }

    /* Get members */
    mesh_participant_id_t members[10];
    size_t count;
    EXPECT_EQ(mesh_participant_get_channel_members(
        registry, MESH_CHANNEL_SUBCORTICAL, members, 10, &count), NIMCP_SUCCESS);

    EXPECT_EQ(count, 3);
}

/* ============================================================================
 * Credential Tests
 * ============================================================================ */

TEST_F(MeshParticipantTest, SetCredential) {
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);

    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "cred_test";

    mesh_participant_id_t id;
    EXPECT_EQ(mesh_participant_register(registry, &iface, &config, &id), NIMCP_SUCCESS);

    /* Initially no credential */
    EXPECT_EQ(mesh_participant_get_credential(registry, id), nullptr);

    /* Set credential */
    credential_t cred = {0};
    cred.state = CREDENTIAL_STATE_VALID;
    cred.privilege_level = 10;

    EXPECT_EQ(mesh_participant_set_credential(registry, id, &cred), NIMCP_SUCCESS);

    const credential_t* retrieved = mesh_participant_get_credential(registry, id);
    EXPECT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->state, CREDENTIAL_STATE_VALID);
    EXPECT_EQ(retrieved->privilege_level, 10);
}

TEST_F(MeshParticipantTest, ValidateCredential) {
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);

    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "validate_test";

    mesh_participant_id_t id;
    EXPECT_EQ(mesh_participant_register(registry, &iface, &config, &id), NIMCP_SUCCESS);

    credential_t cred = {0};
    cred.state = CREDENTIAL_STATE_VALID;
    EXPECT_EQ(mesh_participant_set_credential(registry, id, &cred), NIMCP_SUCCESS);

    EXPECT_TRUE(mesh_participant_validate_credential(registry, id));
}

TEST_F(MeshParticipantTest, SuspendCredential) {
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);

    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "suspend_test";

    mesh_participant_id_t id;
    EXPECT_EQ(mesh_participant_register(registry, &iface, &config, &id), NIMCP_SUCCESS);

    credential_t cred = {0};
    cred.state = CREDENTIAL_STATE_VALID;
    EXPECT_EQ(mesh_participant_set_credential(registry, id, &cred), NIMCP_SUCCESS);

    EXPECT_EQ(mesh_participant_suspend_credential(registry, id, "test suspension"), NIMCP_SUCCESS);

    const credential_t* retrieved = mesh_participant_get_credential(registry, id);
    EXPECT_EQ(retrieved->state, CREDENTIAL_STATE_SUSPENDED);
    EXPECT_FALSE(mesh_participant_validate_credential(registry, id));
}

TEST_F(MeshParticipantTest, RevokeCredential) {
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);

    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "revoke_test";

    mesh_participant_id_t id;
    EXPECT_EQ(mesh_participant_register(registry, &iface, &config, &id), NIMCP_SUCCESS);

    credential_t cred = {0};
    cred.state = CREDENTIAL_STATE_VALID;
    EXPECT_EQ(mesh_participant_set_credential(registry, id, &cred), NIMCP_SUCCESS);

    EXPECT_EQ(mesh_participant_revoke_credential(registry, id, "test revocation"), NIMCP_SUCCESS);

    const credential_t* retrieved = mesh_participant_get_credential(registry, id);
    EXPECT_EQ(retrieved->state, CREDENTIAL_STATE_REVOKED);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(MeshParticipantTest, GetStatistics) {
    /* Register a few participants */
    for (int i = 0; i < 5; i++) {
        mesh_participant_interface_t iface;
        mesh_participant_interface_init(&iface);

        mesh_participant_config_t config;
        mesh_participant_config_init(&config);
        char name[32];
        snprintf(name, sizeof(name), "stats_%d", i);
        config.module_name = name;
        config.type = (i == 0) ? MESH_PARTICIPANT_COORDINATOR : MESH_PARTICIPANT_MODULE;

        mesh_participant_id_t id;
        mesh_participant_register(registry, &iface, &config, &id);
    }

    mesh_registry_stats_t stats;
    EXPECT_EQ(mesh_registry_get_stats(registry, &stats), NIMCP_SUCCESS);

    EXPECT_EQ(stats.total_participants, 5);
    EXPECT_EQ(stats.coordinators, 1);
    EXPECT_EQ(stats.registrations, 5);
}

/* ============================================================================
 * Iteration Tests
 * ============================================================================ */

static bool count_callback(const mesh_participant_interface_t* iface, void* ctx) {
    (void)iface;
    int* count = (int*)ctx;
    (*count)++;
    return true;
}

static bool stop_early_callback(const mesh_participant_interface_t* iface, void* ctx) {
    (void)iface;
    int* count = (int*)ctx;
    (*count)++;
    return (*count < 2);  /* Stop after 2 */
}

TEST_F(MeshParticipantTest, IterateAll) {
    /* Register 5 participants */
    for (int i = 0; i < 5; i++) {
        mesh_participant_interface_t iface;
        mesh_participant_interface_init(&iface);

        mesh_participant_config_t config;
        mesh_participant_config_init(&config);
        char name[32];
        snprintf(name, sizeof(name), "iter_%d", i);
        config.module_name = name;

        mesh_participant_id_t id;
        mesh_participant_register(registry, &iface, &config, &id);
    }

    int count = 0;
    EXPECT_EQ(mesh_registry_iterate(registry, count_callback, &count), NIMCP_SUCCESS);
    EXPECT_EQ(count, 5);
}

TEST_F(MeshParticipantTest, IterateStopEarly) {
    for (int i = 0; i < 5; i++) {
        mesh_participant_interface_t iface;
        mesh_participant_interface_init(&iface);

        mesh_participant_config_t config;
        mesh_participant_config_init(&config);
        char name[32];
        snprintf(name, sizeof(name), "early_%d", i);
        config.module_name = name;

        mesh_participant_id_t id;
        mesh_participant_register(registry, &iface, &config, &id);
    }

    int count = 0;
    EXPECT_EQ(mesh_registry_iterate(registry, stop_early_callback, &count), NIMCP_SUCCESS);
    EXPECT_EQ(count, 2);
}

/* ============================================================================
 * Utility Tests
 * ============================================================================ */

TEST(MeshParticipantUtilTest, InterfaceInit) {
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);

    EXPECT_EQ(iface.id, 0);
    EXPECT_EQ(iface.type, MESH_PARTICIPANT_MODULE);
    EXPECT_EQ(iface.credential, nullptr);
}

TEST(MeshParticipantUtilTest, ConfigInit) {
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);

    EXPECT_EQ(config.module_name, nullptr);
    EXPECT_EQ(config.type, MESH_PARTICIPANT_MODULE);
    EXPECT_EQ(config.home_channel, MESH_CHANNEL_SYSTEM);
}
