/**
 * @file test_mesh_participant_registration.cpp
 * @brief Tests for mesh integration (P3-4/5/8)
 *
 * WHAT: Verify mesh participant registration and adapter framework
 * WHY:  P3-4/5/8 added mesh participant integration for modules
 * HOW:  Test interface init, config init, register/unregister lifecycle
 *
 * Function signatures tested (from include/mesh/nimcp_mesh_participant.h):
 *   void mesh_participant_interface_init(mesh_participant_interface_t* interface);
 *   void mesh_participant_config_init(mesh_participant_config_t* config);
 *   nimcp_error_t mesh_registry_default_config(mesh_registry_config_t* config);
 *   mesh_participant_registry_t* mesh_registry_create(const mesh_registry_config_t* config);
 *   void mesh_registry_destroy(mesh_participant_registry_t* registry);
 *   nimcp_error_t mesh_participant_register(
 *       mesh_participant_registry_t* registry,
 *       const mesh_participant_interface_t* interface,
 *       const mesh_participant_config_t* config,
 *       mesh_participant_id_t* id_out);
 *   nimcp_error_t mesh_participant_unregister(
 *       mesh_participant_registry_t* registry,
 *       mesh_participant_id_t id);
 *   bool mesh_participant_is_registered(
 *       mesh_participant_registry_t* registry,
 *       mesh_participant_id_t id);
 *   nimcp_error_t mesh_registry_get_stats(
 *       mesh_participant_registry_t* registry,
 *       mesh_registry_stats_t* stats);
 *
 * Adapter categories (from include/mesh/nimcp_mesh_adapter.h):
 *   mesh_adapter_category_t:
 *     MESH_ADAPTER_CATEGORY_COGNITIVE, _PERCEPTION, _SUBCORTICAL, etc.
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/error/nimcp_error_codes.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class MeshParticipantRegistrationTest : public ::testing::Test {
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
 * Interface Init Tests
 * ============================================================================ */

TEST_F(MeshParticipantRegistrationTest, InterfaceInitSetsDefaults) {
    mesh_participant_interface_t iface;
    memset(&iface, 0xFF, sizeof(iface)); // Fill with garbage

    mesh_participant_interface_init(&iface);

    // After init, callbacks should be NULL (optional)
    EXPECT_EQ(iface.on_proposal, nullptr);
    EXPECT_EQ(iface.on_endorse_request, nullptr);
    EXPECT_EQ(iface.on_commit, nullptr);
    EXPECT_EQ(iface.on_belief_received, nullptr);
    EXPECT_EQ(iface.on_consensus_reached, nullptr);
    EXPECT_EQ(iface.get_free_energy, nullptr);
    EXPECT_EQ(iface.get_beliefs, nullptr);
    EXPECT_EQ(iface.get_health_metrics, nullptr);
    EXPECT_EQ(iface.gpu_process, nullptr);
    EXPECT_EQ(iface.user_context, nullptr);
    EXPECT_FALSE(iface.has_gpu_acceleration);
    EXPECT_EQ(iface.channel_membership_count, 0u);
}

/* ============================================================================
 * Config Init Tests
 * ============================================================================ */

TEST_F(MeshParticipantRegistrationTest, ConfigInitSetsDefaults) {
    mesh_participant_config_t config;
    memset(&config, 0xFF, sizeof(config)); // Fill with garbage

    mesh_participant_config_init(&config);

    EXPECT_EQ(config.module_name, nullptr);
    EXPECT_EQ(config.user_context, nullptr);
    EXPECT_FALSE(config.request_gpu);
}

/* ============================================================================
 * Register / Unregister Lifecycle Tests
 * ============================================================================ */

TEST_F(MeshParticipantRegistrationTest, RegisterAndUnregister) {
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "test_module", MESH_MAX_NAME_LEN - 1);
    iface.module_name[MESH_MAX_NAME_LEN - 1] = '\0';

    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "test_module";
    config.type = MESH_PARTICIPANT_MODULE;

    mesh_participant_id_t id = 0;
    nimcp_error_t rc = mesh_participant_register(registry, &iface, &config, &id);
    EXPECT_EQ(rc, NIMCP_SUCCESS);
    EXPECT_NE(id, 0u);

    // Verify it is registered
    EXPECT_TRUE(mesh_participant_is_registered(registry, id));

    // Unregister
    rc = mesh_participant_unregister(registry, id);
    EXPECT_EQ(rc, NIMCP_SUCCESS);

    // Verify it is no longer registered
    EXPECT_FALSE(mesh_participant_is_registered(registry, id));
}

TEST_F(MeshParticipantRegistrationTest, RegisterMultipleParticipants) {
    const int num_participants = 5;
    mesh_participant_id_t ids[5];

    for (int i = 0; i < num_participants; i++) {
        mesh_participant_interface_t iface;
        mesh_participant_interface_init(&iface);
        snprintf(iface.module_name, MESH_MAX_NAME_LEN, "module_%d", i);

        mesh_participant_config_t config;
        mesh_participant_config_init(&config);
        config.module_name = iface.module_name;
        config.type = MESH_PARTICIPANT_MODULE;

        nimcp_error_t rc = mesh_participant_register(registry, &iface, &config, &ids[i]);
        EXPECT_EQ(rc, NIMCP_SUCCESS) << "Failed to register participant " << i;
        EXPECT_NE(ids[i], 0u);
    }

    // All should be registered
    for (int i = 0; i < num_participants; i++) {
        EXPECT_TRUE(mesh_participant_is_registered(registry, ids[i]));
    }

    // Unregister all
    for (int i = 0; i < num_participants; i++) {
        nimcp_error_t rc = mesh_participant_unregister(registry, ids[i]);
        EXPECT_EQ(rc, NIMCP_SUCCESS);
    }
}

/* ============================================================================
 * Registry Stats Tests
 * ============================================================================ */

TEST_F(MeshParticipantRegistrationTest, GetStatsEmpty) {
    mesh_registry_stats_t stats;
    memset(&stats, 0xFF, sizeof(stats));

    nimcp_error_t rc = mesh_registry_get_stats(registry, &stats);
    EXPECT_EQ(rc, NIMCP_SUCCESS);
    EXPECT_EQ(stats.total_participants, 0u);
}

TEST_F(MeshParticipantRegistrationTest, GetStatsAfterRegistration) {
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "stats_test", MESH_MAX_NAME_LEN - 1);

    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "stats_test";
    config.type = MESH_PARTICIPANT_MODULE;

    mesh_participant_id_t id = 0;
    mesh_participant_register(registry, &iface, &config, &id);

    mesh_registry_stats_t stats;
    nimcp_error_t rc = mesh_registry_get_stats(registry, &stats);
    EXPECT_EQ(rc, NIMCP_SUCCESS);
    EXPECT_GE(stats.total_participants, 1u);
    EXPECT_GE(stats.registrations, 1u);

    mesh_participant_unregister(registry, id);
}

/* ============================================================================
 * Adapter Channel Categories Tests
 * ============================================================================ */

TEST(MeshAdapterCategoryTest, CategoryEnumValues) {
    // Verify all adapter categories are distinct
    EXPECT_NE(MESH_ADAPTER_CATEGORY_COGNITIVE, MESH_ADAPTER_CATEGORY_PERCEPTION);
    EXPECT_NE(MESH_ADAPTER_CATEGORY_SUBCORTICAL, MESH_ADAPTER_CATEGORY_MOTOR);
    EXPECT_NE(MESH_ADAPTER_CATEGORY_MEMORY, MESH_ADAPTER_CATEGORY_SECURITY);
    EXPECT_NE(MESH_ADAPTER_CATEGORY_SWARM, MESH_ADAPTER_CATEGORY_GPU);
    EXPECT_NE(MESH_ADAPTER_CATEGORY_PLASTICITY, MESH_ADAPTER_CATEGORY_GLIAL);
    EXPECT_NE(MESH_ADAPTER_CATEGORY_SYSTEM, MESH_ADAPTER_CATEGORY_COGNITIVE);
}

/* ============================================================================
 * Null Parameter Tests
 * ============================================================================ */

TEST_F(MeshParticipantRegistrationTest, RegisterNullInterface) {
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "null_test";
    config.type = MESH_PARTICIPANT_MODULE;

    mesh_participant_id_t id = 0;
    nimcp_error_t rc = mesh_participant_register(registry, nullptr, &config, &id);
    EXPECT_NE(rc, NIMCP_SUCCESS);
}

TEST_F(MeshParticipantRegistrationTest, UnregisterInvalidId) {
    // Unregistering a non-existent ID should fail gracefully
    nimcp_error_t rc = mesh_participant_unregister(registry, 99999);
    EXPECT_NE(rc, NIMCP_SUCCESS);
}

TEST_F(MeshParticipantRegistrationTest, IsRegisteredInvalidId) {
    EXPECT_FALSE(mesh_participant_is_registered(registry, 99999));
}

/* ============================================================================
 * Registry Lifecycle Tests
 * ============================================================================ */

TEST(MeshRegistryLifecycleTest, CreateWithNullConfig) {
    mesh_participant_registry_t* reg = mesh_registry_create(nullptr);
    EXPECT_NE(reg, nullptr);
    mesh_registry_destroy(reg);
}

TEST(MeshRegistryLifecycleTest, DestroyNullRegistry) {
    mesh_registry_destroy(nullptr);
    SUCCEED() << "Destroying NULL registry did not crash";
}

TEST(MeshRegistryLifecycleTest, DefaultConfigValid) {
    mesh_registry_config_t config;
    nimcp_error_t rc = mesh_registry_default_config(&config);
    EXPECT_EQ(rc, NIMCP_SUCCESS);
    EXPECT_GT(config.initial_capacity, 0u);
}
