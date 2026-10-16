/**
 * @file test_mesh_module_registry.cpp
 * @brief Unit tests for mesh module registry (Phase 14)
 *
 * Tests type-safe module registration, magic validation, and lookup.
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "mesh/nimcp_mesh_module_registry.h"
#include "mesh/nimcp_mesh_types.h"
#include "utils/memory/nimcp_memory.h"
}

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class MeshModuleRegistryTest : public ::testing::Test {
protected:
    mesh_module_registry_t* registry = nullptr;

    void SetUp() override {
        mesh_module_registry_config_t config;
        mesh_module_registry_default_config(&config);
        config.max_modules = 64;
        config.require_magic_validation = true;
        registry = mesh_module_registry_create(&config);
    }

    void TearDown() override {
        if (registry) {
            mesh_module_registry_destroy(registry);
            registry = nullptr;
        }
    }

    /* Create a fake module with magic number */
    void* create_fake_module(uint32_t magic) {
        struct fake_module {
            uint32_t magic;
            char data[64];
        };
        fake_module* mod = (fake_module*)nimcp_calloc(1, sizeof(fake_module));
        mod->magic = magic;
        return mod;
    }
};

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

TEST(MeshModuleRegistryConfigTest, DefaultConfig) {
    mesh_module_registry_config_t config;
    nimcp_error_t err = mesh_module_registry_default_config(&config);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GT(config.max_modules, 0u);
    EXPECT_TRUE(config.require_magic_validation);
    EXPECT_TRUE(config.enable_duplicate_detection);
}

TEST(MeshModuleRegistryConfigTest, DefaultConfigNullPointer) {
    nimcp_error_t err = mesh_module_registry_default_config(nullptr);
    EXPECT_EQ(err, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Creation/Destruction Tests
 * ============================================================================ */

TEST_F(MeshModuleRegistryTest, CreateDestroy) {
    ASSERT_NE(registry, nullptr);
    /* Destruction is handled in TearDown */
}

TEST(MeshModuleRegistryLifecycleTest, CreateWithDefaults) {
    mesh_module_registry_t* reg = mesh_module_registry_create(nullptr);
    ASSERT_NE(reg, nullptr);
    mesh_module_registry_destroy(reg);
}

TEST(MeshModuleRegistryLifecycleTest, DestroyNull) {
    /* Should not crash */
    mesh_module_registry_destroy(nullptr);
}

/* ============================================================================
 * Registration Tests
 * ============================================================================ */

TEST_F(MeshModuleRegistryTest, RegisterModule) {
    const uint32_t TEST_MAGIC = 0x54455354;  /* "TEST" */
    void* mod = create_fake_module(TEST_MAGIC);

    mesh_module_descriptor_t desc = {
        .module_name = "test_module",
        .category = MESH_ADAPTER_CATEGORY_COGNITIVE,
        .module_instance = mod,
        .module_size = 68,
        .module_magic = TEST_MAGIC,
        .receptive_field = nullptr,
        .health_agent = nullptr,
        .endorser_role = ENDORSER_ROLE_OPTIONAL,
        .policies = nullptr,
        .policy_count = 0,
        .secondary_channels = nullptr,
        .secondary_channel_count = 0
    };

    nimcp_error_t err = mesh_module_registry_register(registry, &desc);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    nimcp_free(mod);
}

TEST_F(MeshModuleRegistryTest, RegisterNullDescriptor) {
    nimcp_error_t err = mesh_module_registry_register(registry, nullptr);
    EXPECT_EQ(err, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(MeshModuleRegistryTest, RegisterNullInstance) {
    mesh_module_descriptor_t desc = {
        .module_name = "test_module",
        .category = MESH_ADAPTER_CATEGORY_COGNITIVE,
        .module_instance = nullptr,
    };

    nimcp_error_t err = mesh_module_registry_register(registry, &desc);
    EXPECT_EQ(err, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(MeshModuleRegistryTest, RegisterDuplicate) {
    const uint32_t TEST_MAGIC = 0x54455354;
    void* mod1 = create_fake_module(TEST_MAGIC);
    void* mod2 = create_fake_module(TEST_MAGIC);

    mesh_module_descriptor_t desc1 = {
        .module_name = "duplicate_module",
        .category = MESH_ADAPTER_CATEGORY_COGNITIVE,
        .module_instance = mod1,
        .module_size = 68,
        .module_magic = TEST_MAGIC,
    };

    mesh_module_descriptor_t desc2 = {
        .module_name = "duplicate_module",
        .category = MESH_ADAPTER_CATEGORY_COGNITIVE,
        .module_instance = mod2,
        .module_size = 68,
        .module_magic = TEST_MAGIC,
    };

    nimcp_error_t err1 = mesh_module_registry_register(registry, &desc1);
    EXPECT_EQ(err1, NIMCP_SUCCESS);

    nimcp_error_t err2 = mesh_module_registry_register(registry, &desc2);
    EXPECT_EQ(err2, NIMCP_ERROR_ALREADY_EXISTS);

    nimcp_free(mod1);
    nimcp_free(mod2);
}

/* ============================================================================
 * Lookup Tests
 * ============================================================================ */

TEST_F(MeshModuleRegistryTest, LookupByName) {
    const uint32_t TEST_MAGIC = 0x4C4F4F4B;  /* "LOOK" */
    void* mod = create_fake_module(TEST_MAGIC);

    mesh_module_descriptor_t desc = {
        .module_name = "lookup_test",
        .category = MESH_ADAPTER_CATEGORY_MEMORY,
        .module_instance = mod,
        .module_size = 68,
        .module_magic = TEST_MAGIC,
    };

    mesh_module_registry_register(registry, &desc);

    const mesh_registered_module_t* found = mesh_module_registry_get(registry, "lookup_test");
    ASSERT_NE(found, nullptr);
    EXPECT_STREQ(found->descriptor.module_name, "lookup_test");
    EXPECT_EQ(found->descriptor.category, MESH_ADAPTER_CATEGORY_MEMORY);

    nimcp_free(mod);
}

TEST_F(MeshModuleRegistryTest, LookupNotFound) {
    const mesh_registered_module_t* found = mesh_module_registry_get(registry, "nonexistent");
    EXPECT_EQ(found, nullptr);
}

TEST_F(MeshModuleRegistryTest, ContainsModule) {
    const uint32_t TEST_MAGIC = 0x434F4E54;  /* "CONT" */
    void* mod = create_fake_module(TEST_MAGIC);

    mesh_module_descriptor_t desc = {
        .module_name = "contains_test",
        .category = MESH_ADAPTER_CATEGORY_MOTOR,
        .module_instance = mod,
        .module_size = 68,
        .module_magic = TEST_MAGIC,
    };

    EXPECT_FALSE(mesh_module_registry_contains(registry, "contains_test"));

    mesh_module_registry_register(registry, &desc);

    EXPECT_TRUE(mesh_module_registry_contains(registry, "contains_test"));

    nimcp_free(mod);
}

/* ============================================================================
 * Unregistration Tests
 * ============================================================================ */

TEST_F(MeshModuleRegistryTest, UnregisterModule) {
    const uint32_t TEST_MAGIC = 0x554E5245;  /* "UNRE" */
    void* mod = create_fake_module(TEST_MAGIC);

    mesh_module_descriptor_t desc = {
        .module_name = "unregister_test",
        .category = MESH_ADAPTER_CATEGORY_SECURITY,
        .module_instance = mod,
        .module_size = 68,
        .module_magic = TEST_MAGIC,
    };

    mesh_module_registry_register(registry, &desc);
    EXPECT_TRUE(mesh_module_registry_contains(registry, "unregister_test"));

    nimcp_error_t err = mesh_module_registry_unregister(registry, "unregister_test");
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_FALSE(mesh_module_registry_contains(registry, "unregister_test"));

    nimcp_free(mod);
}

TEST_F(MeshModuleRegistryTest, UnregisterNonexistent) {
    nimcp_error_t err = mesh_module_registry_unregister(registry, "nonexistent");
    EXPECT_EQ(err, NIMCP_ERROR_NOT_FOUND);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(MeshModuleRegistryTest, GetStats) {
    const uint32_t MAGIC1 = 0x53544131;
    const uint32_t MAGIC2 = 0x53544132;
    void* mod1 = create_fake_module(MAGIC1);
    void* mod2 = create_fake_module(MAGIC2);

    mesh_module_descriptor_t desc1 = {
        .module_name = "stats_module_1",
        .category = MESH_ADAPTER_CATEGORY_COGNITIVE,
        .module_instance = mod1,
        .module_size = 68,
        .module_magic = MAGIC1,
    };

    mesh_module_descriptor_t desc2 = {
        .module_name = "stats_module_2",
        .category = MESH_ADAPTER_CATEGORY_MEMORY,
        .module_instance = mod2,
        .module_size = 68,
        .module_magic = MAGIC2,
    };

    mesh_module_registry_register(registry, &desc1);
    mesh_module_registry_register(registry, &desc2);

    mesh_module_registry_stats_t stats;
    nimcp_error_t err = mesh_module_registry_get_stats(registry, &stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(stats.total_registered, 2u);
    EXPECT_EQ(stats.cognitive_count, 1u);
    EXPECT_EQ(stats.memory_count, 1u);

    nimcp_free(mod1);
    nimcp_free(mod2);
}

/* ============================================================================
 * Instance Access Tests
 * ============================================================================ */

TEST_F(MeshModuleRegistryTest, GetInstance) {
    const uint32_t TEST_MAGIC = 0x494E5354;  /* "INST" */
    void* mod = create_fake_module(TEST_MAGIC);

    mesh_module_descriptor_t desc = {
        .module_name = "instance_test",
        .category = MESH_ADAPTER_CATEGORY_PLASTICITY,
        .module_instance = mod,
        .module_size = 68,
        .module_magic = TEST_MAGIC,
    };

    mesh_module_registry_register(registry, &desc);

    void* retrieved = mesh_module_registry_get_instance(registry, "instance_test");
    EXPECT_EQ(retrieved, mod);

    nimcp_free(mod);
}

TEST_F(MeshModuleRegistryTest, GetInstanceNotFound) {
    void* retrieved = mesh_module_registry_get_instance(registry, "nonexistent");
    EXPECT_EQ(retrieved, nullptr);
}
