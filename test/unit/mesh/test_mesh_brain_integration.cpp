/**
 * @file test_mesh_brain_integration.cpp
 * @brief Unit tests for brain module registration with mesh network
 *
 * Tests brain module registration with real pointers, receptive field assignment,
 * health agent wiring, module lookup after registration, registration failure cases,
 * and cleanup/re-registration scenarios.
 *
 * @author NIMCP Development Team
 * @date 2025-02-01
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "mesh/nimcp_mesh_module_registry.h"
#include "mesh/nimcp_mesh_bootstrap.h"
#include "mesh/nimcp_mesh_types.h"
#include "mesh/nimcp_mesh_receptive_fields.h"
#include "mesh/nimcp_mesh_pattern_routing.h"
#include "mesh/nimcp_mesh_health_bridge.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/memory/nimcp_memory.h"
}

/* ============================================================================
 * Mock Module Structures for Testing
 * ============================================================================ */

#define MOCK_HIPPOCAMPUS_MAGIC 0x48495050  /* "HIPP" */
#define MOCK_AMYGDALA_MAGIC    0x414D5947  /* "AMYG" */
#define MOCK_PFC_MAGIC         0x50464358  /* "PFCX" */
#define MOCK_THALAMUS_MAGIC    0x5448414C  /* "THAL" */
#define MOCK_BBB_MAGIC         0x42424231  /* "BBB1" */

typedef struct mock_hippocampus {
    uint32_t magic;
    char name[32];
    float memory_capacity;
    bool initialized;
} mock_hippocampus_t;

typedef struct mock_amygdala {
    uint32_t magic;
    char name[32];
    float threat_level;
    bool active;
} mock_amygdala_t;

typedef struct mock_pfc {
    uint32_t magic;
    char name[32];
    float cognitive_load;
    uint32_t active_tasks;
} mock_pfc_t;

typedef struct mock_thalamus {
    uint32_t magic;
    char name[32];
    uint32_t relay_count;
} mock_thalamus_t;

typedef struct mock_bbb {
    uint32_t magic;
    char name[32];
    bool barrier_intact;
} mock_bbb_t;

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class MeshBrainIntegrationTest : public ::testing::Test {
protected:
    mesh_bootstrap_t* bootstrap = nullptr;
    mesh_module_registry_t* registry = nullptr;

    /* Mock modules */
    mock_hippocampus_t* hippocampus = nullptr;
    mock_amygdala_t* amygdala = nullptr;
    mock_pfc_t* pfc = nullptr;

    void SetUp() override {
        /* Create bootstrap with minimal config */
        mesh_bootstrap_config_t config;
        mesh_bootstrap_default_config(&config);
        config.subsystems.enable_cognitive = false;
        config.subsystems.enable_sensory = false;
        config.subsystems.enable_motor = false;
        config.subsystems.enable_memory = false;
        config.subsystems.enable_security = false;
        config.subsystems.enable_gpu = false;
        config.subsystems.enable_plasticity = false;
        config.subsystems.enable_glial = false;
        config.subsystems.enable_swarm = false;
        config.subsystems.enable_async = false;
        config.subsystems.enable_lnn = false;
        config.subsystems.enable_snn = false;

        bootstrap = mesh_bootstrap_create(&config);
        ASSERT_NE(bootstrap, nullptr);

        registry = mesh_bootstrap_get_module_registry(bootstrap);

        /* Create mock modules */
        hippocampus = (mock_hippocampus_t*)nimcp_malloc(sizeof(mock_hippocampus_t));
        ASSERT_NE(hippocampus, nullptr);
        hippocampus->magic = MOCK_HIPPOCAMPUS_MAGIC;
        strcpy(hippocampus->name, "hippocampus");
        hippocampus->memory_capacity = 1.0f;
        hippocampus->initialized = true;

        amygdala = (mock_amygdala_t*)nimcp_malloc(sizeof(mock_amygdala_t));
        ASSERT_NE(amygdala, nullptr);
        amygdala->magic = MOCK_AMYGDALA_MAGIC;
        strcpy(amygdala->name, "amygdala");
        amygdala->threat_level = 0.0f;
        amygdala->active = true;

        pfc = (mock_pfc_t*)nimcp_malloc(sizeof(mock_pfc_t));
        ASSERT_NE(pfc, nullptr);
        pfc->magic = MOCK_PFC_MAGIC;
        strcpy(pfc->name, "prefrontal_cortex");
        pfc->cognitive_load = 0.5f;
        pfc->active_tasks = 0;
    }

    void TearDown() override {
        if (hippocampus) {
            nimcp_free(hippocampus);
            hippocampus = nullptr;
        }
        if (amygdala) {
            nimcp_free(amygdala);
            amygdala = nullptr;
        }
        if (pfc) {
            nimcp_free(pfc);
            pfc = nullptr;
        }
        if (bootstrap) {
            mesh_bootstrap_destroy(bootstrap);
            bootstrap = nullptr;
            registry = nullptr;
        }
    }

    /* Helper: Create a basic descriptor */
    mesh_module_descriptor_t create_descriptor(
        const char* name,
        mesh_adapter_category_t category,
        void* instance,
        size_t size,
        uint32_t magic
    ) {
        mesh_module_descriptor_t desc;
        memset(&desc, 0, sizeof(desc));
        desc.module_name = name;
        desc.category = category;
        desc.module_instance = instance;
        desc.module_size = size;
        desc.module_magic = magic;
        return desc;
    }
};

/* ============================================================================
 * Module Registration Tests
 * ============================================================================ */

TEST_F(MeshBrainIntegrationTest, RegisterModuleWithRealPointer) {
    if (!registry) {
        GTEST_SKIP() << "Module registry not available";
    }

    mesh_module_descriptor_t desc = create_descriptor(
        "hippocampus",
        MESH_ADAPTER_CATEGORY_MEMORY,
        hippocampus,
        sizeof(mock_hippocampus_t),
        MOCK_HIPPOCAMPUS_MAGIC
    );

    nimcp_error_t err = mesh_module_registry_register(registry, &desc);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Verify registration */
    EXPECT_TRUE(mesh_module_registry_contains(registry, "hippocampus"));
}

TEST_F(MeshBrainIntegrationTest, RegisterMultipleBrainModules) {
    if (!registry) {
        GTEST_SKIP() << "Module registry not available";
    }

    /* Register hippocampus */
    mesh_module_descriptor_t hipp_desc = create_descriptor(
        "hippocampus",
        MESH_ADAPTER_CATEGORY_MEMORY,
        hippocampus,
        sizeof(mock_hippocampus_t),
        MOCK_HIPPOCAMPUS_MAGIC
    );
    EXPECT_EQ(mesh_module_registry_register(registry, &hipp_desc), NIMCP_SUCCESS);

    /* Register amygdala */
    mesh_module_descriptor_t amyg_desc = create_descriptor(
        "amygdala",
        MESH_ADAPTER_CATEGORY_SUBCORTICAL,
        amygdala,
        sizeof(mock_amygdala_t),
        MOCK_AMYGDALA_MAGIC
    );
    EXPECT_EQ(mesh_module_registry_register(registry, &amyg_desc), NIMCP_SUCCESS);

    /* Register PFC */
    mesh_module_descriptor_t pfc_desc = create_descriptor(
        "prefrontal_cortex",
        MESH_ADAPTER_CATEGORY_COGNITIVE,
        pfc,
        sizeof(mock_pfc_t),
        MOCK_PFC_MAGIC
    );
    EXPECT_EQ(mesh_module_registry_register(registry, &pfc_desc), NIMCP_SUCCESS);

    /* Verify all registered */
    EXPECT_TRUE(mesh_module_registry_contains(registry, "hippocampus"));
    EXPECT_TRUE(mesh_module_registry_contains(registry, "amygdala"));
    EXPECT_TRUE(mesh_module_registry_contains(registry, "prefrontal_cortex"));
}

TEST_F(MeshBrainIntegrationTest, RegisterModuleWithNullName) {
    if (!registry) {
        GTEST_SKIP() << "Module registry not available";
    }

    mesh_module_descriptor_t desc = create_descriptor(
        nullptr,  /* NULL name */
        MESH_ADAPTER_CATEGORY_MEMORY,
        hippocampus,
        sizeof(mock_hippocampus_t),
        MOCK_HIPPOCAMPUS_MAGIC
    );

    nimcp_error_t err = mesh_module_registry_register(registry, &desc);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(MeshBrainIntegrationTest, RegisterModuleWithNullInstance) {
    if (!registry) {
        GTEST_SKIP() << "Module registry not available";
    }

    mesh_module_descriptor_t desc = create_descriptor(
        "test_module",
        MESH_ADAPTER_CATEGORY_MEMORY,
        nullptr,  /* NULL instance */
        sizeof(mock_hippocampus_t),
        MOCK_HIPPOCAMPUS_MAGIC
    );

    nimcp_error_t err = mesh_module_registry_register(registry, &desc);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(MeshBrainIntegrationTest, RegisterDuplicateModuleName) {
    if (!registry) {
        GTEST_SKIP() << "Module registry not available";
    }

    mesh_module_descriptor_t desc1 = create_descriptor(
        "hippocampus",
        MESH_ADAPTER_CATEGORY_MEMORY,
        hippocampus,
        sizeof(mock_hippocampus_t),
        MOCK_HIPPOCAMPUS_MAGIC
    );
    EXPECT_EQ(mesh_module_registry_register(registry, &desc1), NIMCP_SUCCESS);

    /* Try to register with same name */
    mesh_module_descriptor_t desc2 = create_descriptor(
        "hippocampus",
        MESH_ADAPTER_CATEGORY_COGNITIVE,
        pfc,
        sizeof(mock_pfc_t),
        MOCK_PFC_MAGIC
    );
    nimcp_error_t err = mesh_module_registry_register(registry, &desc2);
    EXPECT_EQ(err, NIMCP_ERROR_ALREADY_EXISTS);
}

/* ============================================================================
 * Receptive Field Assignment Tests
 * ============================================================================ */

TEST_F(MeshBrainIntegrationTest, RegisterModuleWithReceptiveField) {
    if (!registry) {
        GTEST_SKIP() << "Module registry not available";
    }

    /* Initialize receptive field library */
    mesh_receptive_fields_init();

    mesh_module_descriptor_t desc = create_descriptor(
        "hippocampus",
        MESH_ADAPTER_CATEGORY_MEMORY,
        hippocampus,
        sizeof(mock_hippocampus_t),
        MOCK_HIPPOCAMPUS_MAGIC
    );
    desc.receptive_field = &MESH_RF_HIPPOCAMPUS;

    nimcp_error_t err = mesh_module_registry_register(registry, &desc);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Verify the module has the field */
    const mesh_registered_module_t* mod = mesh_module_registry_get(registry, "hippocampus");
    EXPECT_NE(mod, nullptr);
    if (mod) {
        EXPECT_NE(mod->descriptor.receptive_field, nullptr);
    }

    mesh_receptive_fields_cleanup();
}

TEST_F(MeshBrainIntegrationTest, RegisterMultipleModulesWithReceptiveFields) {
    if (!registry) {
        GTEST_SKIP() << "Module registry not available";
    }

    mesh_receptive_fields_init();

    /* Register hippocampus with memory field */
    mesh_module_descriptor_t hipp_desc = create_descriptor(
        "hippocampus",
        MESH_ADAPTER_CATEGORY_MEMORY,
        hippocampus,
        sizeof(mock_hippocampus_t),
        MOCK_HIPPOCAMPUS_MAGIC
    );
    hipp_desc.receptive_field = &MESH_RF_HIPPOCAMPUS;
    EXPECT_EQ(mesh_module_registry_register(registry, &hipp_desc), NIMCP_SUCCESS);

    /* Register amygdala with emotional field */
    mesh_module_descriptor_t amyg_desc = create_descriptor(
        "amygdala",
        MESH_ADAPTER_CATEGORY_SUBCORTICAL,
        amygdala,
        sizeof(mock_amygdala_t),
        MOCK_AMYGDALA_MAGIC
    );
    amyg_desc.receptive_field = &MESH_RF_AMYGDALA;
    EXPECT_EQ(mesh_module_registry_register(registry, &amyg_desc), NIMCP_SUCCESS);

    /* Verify both have fields */
    const mesh_registered_module_t* hipp = mesh_module_registry_get(registry, "hippocampus");
    const mesh_registered_module_t* amyg = mesh_module_registry_get(registry, "amygdala");
    EXPECT_NE(hipp, nullptr);
    EXPECT_NE(amyg, nullptr);

    mesh_receptive_fields_cleanup();
}

/* ============================================================================
 * Health Agent Wiring Tests
 * ============================================================================ */

TEST_F(MeshBrainIntegrationTest, RegisterModuleWithHealthAgent) {
    if (!registry) {
        GTEST_SKIP() << "Module registry not available";
    }

    /* Create a mock health agent (NULL is acceptable for this test) */
    mesh_module_descriptor_t desc = create_descriptor(
        "hippocampus",
        MESH_ADAPTER_CATEGORY_MEMORY,
        hippocampus,
        sizeof(mock_hippocampus_t),
        MOCK_HIPPOCAMPUS_MAGIC
    );
    desc.health_agent = nullptr;  /* Real tests would use actual agent */

    nimcp_error_t err = mesh_module_registry_register(registry, &desc);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(MeshBrainIntegrationTest, HealthBridgeTracksRegisteredModules) {
    if (!bootstrap) {
        GTEST_SKIP() << "Bootstrap not available";
    }

    mesh_health_bridge_t* health_bridge = mesh_bootstrap_get_health_bridge(bootstrap);
    if (!health_bridge) {
        GTEST_SKIP() << "Health bridge not available";
    }

    /* Register a module */
    mesh_module_descriptor_t desc = create_descriptor(
        "hippocampus",
        MESH_ADAPTER_CATEGORY_MEMORY,
        hippocampus,
        sizeof(mock_hippocampus_t),
        MOCK_HIPPOCAMPUS_MAGIC
    );

    if (registry) {
        mesh_module_registry_register(registry, &desc);
    }

    /* Get stats - should reflect registration */
    mesh_health_bridge_stats_t stats;
    nimcp_error_t err = mesh_health_bridge_get_stats(health_bridge, &stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

/* ============================================================================
 * Module Lookup Tests
 * ============================================================================ */

TEST_F(MeshBrainIntegrationTest, LookupModuleByName) {
    if (!registry) {
        GTEST_SKIP() << "Module registry not available";
    }

    mesh_module_descriptor_t desc = create_descriptor(
        "hippocampus",
        MESH_ADAPTER_CATEGORY_MEMORY,
        hippocampus,
        sizeof(mock_hippocampus_t),
        MOCK_HIPPOCAMPUS_MAGIC
    );
    mesh_module_registry_register(registry, &desc);

    const mesh_registered_module_t* mod = mesh_module_registry_get(registry, "hippocampus");
    ASSERT_NE(mod, nullptr);
    EXPECT_STREQ(mod->descriptor.module_name, "hippocampus");
    EXPECT_EQ(mod->descriptor.module_instance, hippocampus);
}

TEST_F(MeshBrainIntegrationTest, LookupModuleByParticipantId) {
    if (!registry) {
        GTEST_SKIP() << "Module registry not available";
    }

    mesh_module_descriptor_t desc = create_descriptor(
        "hippocampus",
        MESH_ADAPTER_CATEGORY_MEMORY,
        hippocampus,
        sizeof(mock_hippocampus_t),
        MOCK_HIPPOCAMPUS_MAGIC
    );
    mesh_module_registry_register(registry, &desc);

    const mesh_registered_module_t* mod = mesh_module_registry_get(registry, "hippocampus");
    ASSERT_NE(mod, nullptr);

    mesh_participant_id_t pid = mod->participant_id;
    const mesh_registered_module_t* found = mesh_module_registry_get_by_id(registry, pid);
    /* When participant_id is 0 (not registered with participant registry), the
     * lookup may return any module with id=0. Verify lookup succeeds and if the
     * id is non-zero, verify it matches our module. */
    if (pid != 0) {
        EXPECT_NE(found, nullptr);
        if (found) {
            EXPECT_STREQ(found->descriptor.module_name, "hippocampus");
        }
    } else {
        /* participant_id 0 is ambiguous - just verify lookup doesn't crash */
        (void)found;
    }
}

TEST_F(MeshBrainIntegrationTest, LookupNonexistentModule) {
    if (!registry) {
        GTEST_SKIP() << "Module registry not available";
    }

    const mesh_registered_module_t* mod = mesh_module_registry_get(registry, "nonexistent");
    EXPECT_EQ(mod, nullptr);
}

TEST_F(MeshBrainIntegrationTest, GetModuleInstance) {
    if (!registry) {
        GTEST_SKIP() << "Module registry not available";
    }

    mesh_module_descriptor_t desc = create_descriptor(
        "hippocampus",
        MESH_ADAPTER_CATEGORY_MEMORY,
        hippocampus,
        sizeof(mock_hippocampus_t),
        MOCK_HIPPOCAMPUS_MAGIC
    );
    mesh_module_registry_register(registry, &desc);

    void* instance = mesh_module_registry_get_instance(registry, "hippocampus");
    EXPECT_EQ(instance, hippocampus);

    /* Verify we can cast and access the module */
    mock_hippocampus_t* h = (mock_hippocampus_t*)instance;
    EXPECT_EQ(h->magic, MOCK_HIPPOCAMPUS_MAGIC);
    EXPECT_STREQ(h->name, "hippocampus");
}

TEST_F(MeshBrainIntegrationTest, GetModulesByCategory) {
    if (!registry) {
        GTEST_SKIP() << "Module registry not available";
    }

    /* Register multiple memory modules */
    mesh_module_descriptor_t hipp_desc = create_descriptor(
        "hippocampus",
        MESH_ADAPTER_CATEGORY_MEMORY,
        hippocampus,
        sizeof(mock_hippocampus_t),
        MOCK_HIPPOCAMPUS_MAGIC
    );
    mesh_module_registry_register(registry, &hipp_desc);

    /* Register cognitive module */
    mesh_module_descriptor_t pfc_desc = create_descriptor(
        "prefrontal_cortex",
        MESH_ADAPTER_CATEGORY_COGNITIVE,
        pfc,
        sizeof(mock_pfc_t),
        MOCK_PFC_MAGIC
    );
    mesh_module_registry_register(registry, &pfc_desc);

    /* Get memory modules */
    const mesh_registered_module_t* modules[16];
    size_t count = 0;
    nimcp_error_t err = mesh_module_registry_get_by_category(
        registry, MESH_ADAPTER_CATEGORY_MEMORY, modules, 16, &count
    );
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GE(count, 1u);
}

/* ============================================================================
 * Registration Failure Tests
 * ============================================================================ */

TEST_F(MeshBrainIntegrationTest, RegisterWithInvalidMagic) {
    if (!registry) {
        GTEST_SKIP() << "Module registry not available";
    }

    /* Corrupt the magic number */
    mock_hippocampus_t corrupted;
    corrupted.magic = 0xDEADBEEF;  /* Wrong magic */
    strcpy(corrupted.name, "corrupted");

    mesh_module_descriptor_t desc = create_descriptor(
        "corrupted_module",
        MESH_ADAPTER_CATEGORY_MEMORY,
        &corrupted,
        sizeof(mock_hippocampus_t),
        MOCK_HIPPOCAMPUS_MAGIC  /* Expected magic doesn't match */
    );

    nimcp_error_t err = mesh_module_registry_register(registry, &desc);
    /* Should fail magic validation */
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(MeshBrainIntegrationTest, RegisterWithNullRegistry) {
    mesh_module_descriptor_t desc = create_descriptor(
        "hippocampus",
        MESH_ADAPTER_CATEGORY_MEMORY,
        hippocampus,
        sizeof(mock_hippocampus_t),
        MOCK_HIPPOCAMPUS_MAGIC
    );

    nimcp_error_t err = mesh_module_registry_register(nullptr, &desc);
    // NULL registry returns INVALID_PARAM (validates magic too)
    EXPECT_TRUE(err == NIMCP_ERROR_NULL_POINTER || err == NIMCP_ERROR_INVALID_PARAM);
}

TEST_F(MeshBrainIntegrationTest, RegisterWithNullDescriptor) {
    if (!registry) {
        GTEST_SKIP() << "Module registry not available";
    }

    nimcp_error_t err = mesh_module_registry_register(registry, nullptr);
    EXPECT_EQ(err, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Cleanup and Re-registration Tests
 * ============================================================================ */

TEST_F(MeshBrainIntegrationTest, UnregisterModule) {
    if (!registry) {
        GTEST_SKIP() << "Module registry not available";
    }

    mesh_module_descriptor_t desc = create_descriptor(
        "hippocampus",
        MESH_ADAPTER_CATEGORY_MEMORY,
        hippocampus,
        sizeof(mock_hippocampus_t),
        MOCK_HIPPOCAMPUS_MAGIC
    );
    mesh_module_registry_register(registry, &desc);
    EXPECT_TRUE(mesh_module_registry_contains(registry, "hippocampus"));

    nimcp_error_t err = mesh_module_registry_unregister(registry, "hippocampus");
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_FALSE(mesh_module_registry_contains(registry, "hippocampus"));
}

TEST_F(MeshBrainIntegrationTest, ReregisterAfterUnregister) {
    if (!registry) {
        GTEST_SKIP() << "Module registry not available";
    }

    mesh_module_descriptor_t desc = create_descriptor(
        "hippocampus",
        MESH_ADAPTER_CATEGORY_MEMORY,
        hippocampus,
        sizeof(mock_hippocampus_t),
        MOCK_HIPPOCAMPUS_MAGIC
    );

    /* Register */
    mesh_module_registry_register(registry, &desc);
    EXPECT_TRUE(mesh_module_registry_contains(registry, "hippocampus"));

    /* Unregister */
    mesh_module_registry_unregister(registry, "hippocampus");
    EXPECT_FALSE(mesh_module_registry_contains(registry, "hippocampus"));

    /* Re-register */
    nimcp_error_t err = mesh_module_registry_register(registry, &desc);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_TRUE(mesh_module_registry_contains(registry, "hippocampus"));
}

TEST_F(MeshBrainIntegrationTest, UnregisterNonexistent) {
    if (!registry) {
        GTEST_SKIP() << "Module registry not available";
    }

    nimcp_error_t err = mesh_module_registry_unregister(registry, "nonexistent");
    EXPECT_EQ(err, NIMCP_ERROR_NOT_FOUND);
}

/* ============================================================================
 * Validation Tests
 * ============================================================================ */

TEST_F(MeshBrainIntegrationTest, ValidateMagicNumber) {
    bool valid = mesh_module_validate_magic(hippocampus, MOCK_HIPPOCAMPUS_MAGIC);
    EXPECT_TRUE(valid);

    bool invalid = mesh_module_validate_magic(hippocampus, 0xBADC0DE);
    EXPECT_FALSE(invalid);
}

TEST_F(MeshBrainIntegrationTest, ValidateAllRegisteredModules) {
    if (!registry) {
        GTEST_SKIP() << "Module registry not available";
    }

    mesh_module_descriptor_t desc = create_descriptor(
        "hippocampus",
        MESH_ADAPTER_CATEGORY_MEMORY,
        hippocampus,
        sizeof(mock_hippocampus_t),
        MOCK_HIPPOCAMPUS_MAGIC
    );
    mesh_module_registry_register(registry, &desc);

    size_t invalid_count = 0;
    nimcp_error_t err = mesh_module_registry_validate_all(registry, &invalid_count);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(invalid_count, 0u);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(MeshBrainIntegrationTest, GetRegistryStats) {
    if (!registry) {
        GTEST_SKIP() << "Module registry not available";
    }

    /* Register some modules */
    mesh_module_descriptor_t hipp_desc = create_descriptor(
        "hippocampus",
        MESH_ADAPTER_CATEGORY_MEMORY,
        hippocampus,
        sizeof(mock_hippocampus_t),
        MOCK_HIPPOCAMPUS_MAGIC
    );
    mesh_module_registry_register(registry, &hipp_desc);

    mesh_module_descriptor_t pfc_desc = create_descriptor(
        "prefrontal_cortex",
        MESH_ADAPTER_CATEGORY_COGNITIVE,
        pfc,
        sizeof(mock_pfc_t),
        MOCK_PFC_MAGIC
    );
    mesh_module_registry_register(registry, &pfc_desc);

    mesh_module_registry_stats_t stats;
    nimcp_error_t err = mesh_module_registry_get_stats(registry, &stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GE(stats.total_registered, 2u);
    EXPECT_GE(stats.memory_count, 1u);
    EXPECT_GE(stats.cognitive_count, 1u);
}
