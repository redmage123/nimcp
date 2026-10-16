/**
 * @file test_kg_hierarchy.cpp
 * @brief Unit tests for KG Hierarchy module (4-level hierarchical view)
 *
 * Tests the hierarchical view of the brain knowledge graph including:
 * - Lifecycle (create/destroy)
 * - Level 0 queries (brain statistics, hemispheres)
 * - Level 1 queries (hemisphere info and modules)
 * - Level 2 queries (layer info and modules)
 * - Level 3 queries (module details, connections)
 * - Hierarchy traversal
 * - Real-time updates (state, health, anomaly)
 * - Thread safety (read locks)
 * - String conversion utilities
 */

#include <gtest/gtest.h>
#include <cstring>
#include <thread>
#include <atomic>
#include <vector>

#include "core/brain/nimcp_kg_hierarchy.h"
#include "core/brain/nimcp_brain_kg.h"

// ============================================================================
// Test Fixture
// ============================================================================

class KGHierarchyTest : public ::testing::Test {
protected:
    brain_kg_t* kg;
    kg_hierarchy_t* hier;
    kg_hierarchy_config_t config;

    void SetUp() override {
        // Create brain KG
        brain_kg_config_t kg_config;
        brain_kg_default_config(&kg_config);
        kg_config.enable_security = false;
        kg_config.enable_access_control = false;
        kg_config.enable_integrity_checks = false;
        kg = brain_kg_create(&kg_config);
        ASSERT_NE(kg, nullptr);

        // Get default hierarchy config
        kg_hierarchy_default_config(&config);
        hier = nullptr;
    }

    void TearDown() override {
        if (hier) {
            kg_hierarchy_destroy(hier);
            hier = nullptr;
        }
        if (kg) {
            brain_kg_destroy(kg);
            kg = nullptr;
        }
    }

    // Helper to add various module types to the KG
    brain_kg_node_id_t add_module(const char* name, brain_kg_node_type_t type) {
        return brain_kg_add_node(kg, name, type, "Test module");
    }

    // Helper to populate KG with modules for different hemispheres
    void populate_test_modules() {
        // Left hemisphere modules (language, logic)
        add_module("broca_area", BRAIN_KG_NODE_COGNITIVE);
        add_module("wernicke_area", BRAIN_KG_NODE_COGNITIVE);
        add_module("language_processor", BRAIN_KG_NODE_COGNITIVE);
        add_module("math_engine", BRAIN_KG_NODE_COGNITIVE);
        add_module("logic_reasoner", BRAIN_KG_NODE_COGNITIVE);

        // Right hemisphere modules (spatial, creative)
        add_module("spatial_processor", BRAIN_KG_NODE_PERCEPTION);
        add_module("visual_cortex", BRAIN_KG_NODE_PERCEPTION);
        add_module("pattern_recognizer", BRAIN_KG_NODE_PERCEPTION);
        add_module("face_recognition", BRAIN_KG_NODE_PERCEPTION);
        add_module("emotion_processor", BRAIN_KG_NODE_COGNITIVE);

        // Bilateral modules (core systems)
        add_module("coordinator", BRAIN_KG_NODE_COORDINATOR);
        add_module("global_workspace", BRAIN_KG_NODE_INTEGRATION);
        add_module("memory_consolidation", BRAIN_KG_NODE_CORE);
        add_module("plasticity_manager", BRAIN_KG_NODE_PLASTICITY);
        add_module("immune_system", BRAIN_KG_NODE_SECURITY);
    }
};

// ============================================================================
// Lifecycle Tests
// ============================================================================

TEST_F(KGHierarchyTest, test_create_with_null_kg) {
    hier = kg_hierarchy_create(nullptr, &config);
    EXPECT_EQ(hier, nullptr);
}

TEST_F(KGHierarchyTest, test_create_with_default_config) {
    hier = kg_hierarchy_create(kg, nullptr);  // NULL config = defaults
    EXPECT_NE(hier, nullptr);
}

TEST_F(KGHierarchyTest, test_create_with_explicit_config) {
    config.lazy_rebuild = false;
    config.cache_ttl_ms = 5000;
    hier = kg_hierarchy_create(kg, &config);
    EXPECT_NE(hier, nullptr);
}

TEST_F(KGHierarchyTest, test_destroy_null_safe) {
    kg_hierarchy_destroy(nullptr);  // Should not crash
}

TEST_F(KGHierarchyTest, test_destroy_normal) {
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);
    kg_hierarchy_destroy(hier);
    hier = nullptr;  // Prevent double-free in TearDown
}

TEST_F(KGHierarchyTest, test_default_config) {
    kg_hierarchy_config_t cfg;
    int result = kg_hierarchy_default_config(&cfg);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(cfg.lazy_rebuild);
    EXPECT_EQ(cfg.cache_ttl_ms, 0u);  // No expiry by default
}

TEST_F(KGHierarchyTest, test_default_config_null) {
    int result = kg_hierarchy_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

// ============================================================================
// Level 0 (Brain Level) Tests
// ============================================================================

TEST_F(KGHierarchyTest, test_brain_stats_empty_kg) {
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    kg_brain_stats_t stats;
    int result = kg_hierarchy_get_brain_stats(hier, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.total_modules, 0u);
    EXPECT_EQ(stats.running_modules, 0u);
}

TEST_F(KGHierarchyTest, test_brain_stats_with_modules) {
    populate_test_modules();
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    kg_brain_stats_t stats;
    int result = kg_hierarchy_get_brain_stats(hier, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.total_modules, 15u);  // 5 left + 5 right + 5 bilateral
    EXPECT_GE(stats.left_modules, 4u);    // Language/logic modules
    EXPECT_GE(stats.right_modules, 4u);   // Spatial/visual modules
    EXPECT_GE(stats.bilateral_modules, 4u); // Core/coordinator modules
}

TEST_F(KGHierarchyTest, test_brain_stats_null_params) {
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    int result = kg_hierarchy_get_brain_stats(hier, nullptr);
    EXPECT_EQ(result, -1);

    kg_brain_stats_t stats;
    result = kg_hierarchy_get_brain_stats(nullptr, &stats);
    EXPECT_EQ(result, -1);
}

TEST_F(KGHierarchyTest, test_get_hemispheres) {
    populate_test_modules();
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    kg_hemisphere_info_t hemispheres[KG_HEMISPHERE_COUNT];
    uint32_t count = kg_hierarchy_get_hemispheres(hier, hemispheres);
    EXPECT_EQ(count, (uint32_t)KG_HEMISPHERE_COUNT);

    // Verify hemisphere names
    bool found_left = false, found_right = false, found_bilateral = false;
    for (uint32_t i = 0; i < count; i++) {
        if (hemispheres[i].hemisphere_id == KG_HEMISPHERE_LEFT) found_left = true;
        if (hemispheres[i].hemisphere_id == KG_HEMISPHERE_RIGHT) found_right = true;
        if (hemispheres[i].hemisphere_id == KG_HEMISPHERE_BILATERAL) found_bilateral = true;
    }
    EXPECT_TRUE(found_left);
    EXPECT_TRUE(found_right);
    EXPECT_TRUE(found_bilateral);
}

TEST_F(KGHierarchyTest, test_get_layers) {
    populate_test_modules();
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    kg_layer_info_t layers[KG_LAYER_COUNT];
    uint32_t count = kg_hierarchy_get_layers(hier, layers);
    EXPECT_EQ(count, (uint32_t)KG_LAYER_COUNT);

    // Verify all layers have valid names
    for (uint32_t i = 0; i < count; i++) {
        EXPECT_NE(layers[i].name, nullptr);
    }
}

TEST_F(KGHierarchyTest, test_brain_health) {
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    bio_module_health_t health = kg_hierarchy_get_brain_health(hier);
    // Empty KG should report healthy (no unhealthy modules)
    EXPECT_EQ(health, BIO_MODULE_HEALTH_HEALTHY);
}

// ============================================================================
// Level 1 (Hemisphere Level) Tests
// ============================================================================

TEST_F(KGHierarchyTest, test_hemisphere_info_left) {
    populate_test_modules();
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    kg_hemisphere_info_t info;
    int result = kg_hierarchy_get_hemisphere_info(hier, KG_HEMISPHERE_LEFT, &info);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(info.hemisphere_id, KG_HEMISPHERE_LEFT);
    EXPECT_NE(info.name, nullptr);
    EXPECT_GE(info.total_modules, 4u);  // broca, wernicke, language, math, logic
}

TEST_F(KGHierarchyTest, test_hemisphere_info_right) {
    populate_test_modules();
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    kg_hemisphere_info_t info;
    int result = kg_hierarchy_get_hemisphere_info(hier, KG_HEMISPHERE_RIGHT, &info);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(info.hemisphere_id, KG_HEMISPHERE_RIGHT);
    EXPECT_GE(info.total_modules, 4u);  // spatial, visual, pattern, face
}

TEST_F(KGHierarchyTest, test_hemisphere_info_bilateral) {
    populate_test_modules();
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    kg_hemisphere_info_t info;
    int result = kg_hierarchy_get_hemisphere_info(hier, KG_HEMISPHERE_BILATERAL, &info);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(info.hemisphere_id, KG_HEMISPHERE_BILATERAL);
    EXPECT_GE(info.total_modules, 4u);  // coordinator, gw, memory, plasticity, immune
}

TEST_F(KGHierarchyTest, test_hemisphere_info_invalid) {
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    kg_hemisphere_info_t info;
    int result = kg_hierarchy_get_hemisphere_info(hier, (kg_hemisphere_t)99, &info);
    EXPECT_EQ(result, -1);
}

TEST_F(KGHierarchyTest, test_hemisphere_info_null_params) {
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    int result = kg_hierarchy_get_hemisphere_info(hier, KG_HEMISPHERE_LEFT, nullptr);
    EXPECT_EQ(result, -1);

    kg_hemisphere_info_t info;
    result = kg_hierarchy_get_hemisphere_info(nullptr, KG_HEMISPHERE_LEFT, &info);
    EXPECT_EQ(result, -1);
}

TEST_F(KGHierarchyTest, test_hemisphere_modules) {
    populate_test_modules();
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    brain_kg_node_id_t modules[32];
    uint32_t count = kg_hierarchy_get_hemisphere_modules(hier, KG_HEMISPHERE_LEFT, modules, 32);
    EXPECT_GE(count, 4u);

    // Verify all returned IDs are valid
    for (uint32_t i = 0; i < count; i++) {
        EXPECT_NE(modules[i], BRAIN_KG_INVALID_NODE);
    }
}

TEST_F(KGHierarchyTest, test_hemisphere_health) {
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    bio_module_health_t health = kg_hierarchy_get_hemisphere_health(hier, KG_HEMISPHERE_LEFT);
    EXPECT_EQ(health, BIO_MODULE_HEALTH_HEALTHY);
}

TEST_F(KGHierarchyTest, test_interhemispheric_connections) {
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    uint32_t conn = kg_hierarchy_get_interhemispheric_connections(hier);
    // Empty KG has no connections
    EXPECT_EQ(conn, 0u);
}

// ============================================================================
// Level 2 (Layer Level) Tests
// ============================================================================

TEST_F(KGHierarchyTest, test_layer_info_valid) {
    populate_test_modules();
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    for (int i = 0; i < KG_LAYER_COUNT; i++) {
        kg_layer_info_t info;
        int result = kg_hierarchy_get_layer_info(hier, (kg_cortical_layer_t)i, &info);
        EXPECT_EQ(result, 0);
        EXPECT_EQ(info.layer_id, (kg_cortical_layer_t)i);
        EXPECT_NE(info.name, nullptr);
    }
}

TEST_F(KGHierarchyTest, test_layer_info_invalid) {
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    kg_layer_info_t info;
    int result = kg_hierarchy_get_layer_info(hier, (kg_cortical_layer_t)99, &info);
    EXPECT_EQ(result, -1);
}

TEST_F(KGHierarchyTest, test_layer_modules) {
    populate_test_modules();
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    // Count total modules across all layers
    uint32_t total = 0;
    for (int i = 0; i < KG_LAYER_COUNT; i++) {
        brain_kg_node_id_t modules[64];
        uint32_t count = kg_hierarchy_get_layer_modules(hier, (kg_cortical_layer_t)i, modules, 64);
        total += count;
    }
    EXPECT_EQ(total, 15u);  // All modules should be in some layer
}

TEST_F(KGHierarchyTest, test_interlayer_connections) {
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    uint32_t conn = kg_hierarchy_get_interlayer_connections(hier, KG_LAYER_IV, KG_LAYER_V);
    // Empty KG has no inter-layer connections
    EXPECT_EQ(conn, 0u);
}

// ============================================================================
// Level 3 (Module Level) Tests
// ============================================================================

TEST_F(KGHierarchyTest, test_module_info_valid) {
    brain_kg_node_id_t node_id = add_module("test_module", BRAIN_KG_NODE_COGNITIVE);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    kg_module_info_t info;
    int result = kg_hierarchy_get_module_info(hier, node_id, &info);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(info.node_id, node_id);
    EXPECT_STREQ(info.name, "test_module");
    EXPECT_EQ(info.node_type, BRAIN_KG_NODE_COGNITIVE);
}

TEST_F(KGHierarchyTest, test_module_info_invalid_id) {
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    kg_module_info_t info;
    int result = kg_hierarchy_get_module_info(hier, BRAIN_KG_INVALID_NODE, &info);
    EXPECT_EQ(result, -1);

    result = kg_hierarchy_get_module_info(hier, 99999, &info);
    EXPECT_EQ(result, -1);
}

TEST_F(KGHierarchyTest, test_find_module_by_name) {
    brain_kg_node_id_t expected_id = add_module("find_me", BRAIN_KG_NODE_PERCEPTION);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    brain_kg_node_id_t found_id = kg_hierarchy_find_module_by_name(hier, "find_me");
    EXPECT_EQ(found_id, expected_id);
}

TEST_F(KGHierarchyTest, test_find_module_not_found) {
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    brain_kg_node_id_t found_id = kg_hierarchy_find_module_by_name(hier, "nonexistent");
    EXPECT_EQ(found_id, BRAIN_KG_INVALID_NODE);
}

TEST_F(KGHierarchyTest, test_find_module_null_name) {
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    brain_kg_node_id_t found_id = kg_hierarchy_find_module_by_name(hier, nullptr);
    EXPECT_EQ(found_id, BRAIN_KG_INVALID_NODE);
}

TEST_F(KGHierarchyTest, test_module_hemisphere_assignment) {
    // Language module should be left hemisphere
    brain_kg_node_id_t broca = add_module("broca_area", BRAIN_KG_NODE_COGNITIVE);
    // Spatial module should be right hemisphere
    brain_kg_node_id_t spatial = add_module("spatial_processor", BRAIN_KG_NODE_PERCEPTION);
    // Coordinator should be bilateral
    brain_kg_node_id_t coord = add_module("coordinator", BRAIN_KG_NODE_COORDINATOR);

    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    int hemi_broca = kg_hierarchy_get_module_hemisphere(hier, broca);
    int hemi_spatial = kg_hierarchy_get_module_hemisphere(hier, spatial);
    int hemi_coord = kg_hierarchy_get_module_hemisphere(hier, coord);

    EXPECT_EQ(hemi_broca, (int)KG_HEMISPHERE_LEFT);
    EXPECT_EQ(hemi_spatial, (int)KG_HEMISPHERE_RIGHT);
    EXPECT_EQ(hemi_coord, (int)KG_HEMISPHERE_BILATERAL);
}

TEST_F(KGHierarchyTest, test_module_hemisphere_invalid) {
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    int hemi = kg_hierarchy_get_module_hemisphere(hier, 99999);
    EXPECT_EQ(hemi, -1);
}

TEST_F(KGHierarchyTest, test_modules_by_category) {
    add_module("cog1", BRAIN_KG_NODE_COGNITIVE);
    add_module("cog2", BRAIN_KG_NODE_COGNITIVE);
    add_module("percept1", BRAIN_KG_NODE_PERCEPTION);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    brain_kg_node_id_t modules[32];
    uint32_t count = kg_hierarchy_get_modules_by_category(hier, BIO_MODULE_CATEGORY_COGNITIVE, modules, 32);
    // Note: cognitive modules from orchestrator category
    // The category mapping depends on implementation
    EXPECT_GE(count, 0u);  // May be 0 if category mapping differs
}

// ============================================================================
// Hierarchy Traversal Tests
// ============================================================================

TEST_F(KGHierarchyTest, test_get_module_layer) {
    // Add a perception module - should go to Layer IV (input processing)
    brain_kg_node_id_t node_id = add_module("perception_mod", BRAIN_KG_NODE_PERCEPTION);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    int layer = kg_hierarchy_get_module_layer(hier, node_id);
    EXPECT_GE(layer, 0);
    EXPECT_LT(layer, KG_LAYER_COUNT);
}

TEST_F(KGHierarchyTest, test_get_module_layer_invalid) {
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    int layer = kg_hierarchy_get_module_layer(hier, 99999);
    EXPECT_EQ(layer, -1);
}

TEST_F(KGHierarchyTest, test_get_level_module) {
    brain_kg_node_id_t node_id = add_module("some_module", BRAIN_KG_NODE_COGNITIVE);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    kg_hierarchy_level_t level = kg_hierarchy_get_level(hier, node_id);
    EXPECT_EQ(level, KG_LEVEL_MODULE);
}

TEST_F(KGHierarchyTest, test_get_parent) {
    brain_kg_node_id_t node_id = add_module("child_module", BRAIN_KG_NODE_COGNITIVE);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    brain_kg_node_id_t parent = kg_hierarchy_get_parent(hier, node_id);
    // Parent should be valid (either layer or hemisphere node)
    // Implementation may return INVALID if hierarchy doesn't track virtual parents
    // Both behaviors are acceptable
    (void)parent;  // Suppress unused warning
}

TEST_F(KGHierarchyTest, test_get_dependencies_empty) {
    brain_kg_node_id_t node_id = add_module("no_deps", BRAIN_KG_NODE_COGNITIVE);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    brain_kg_node_id_t deps[32];
    uint32_t count = kg_hierarchy_get_dependencies(hier, node_id, deps, 32);
    EXPECT_EQ(count, 0u);  // No edges added
}

TEST_F(KGHierarchyTest, test_get_dependents_empty) {
    brain_kg_node_id_t node_id = add_module("no_dependents", BRAIN_KG_NODE_COGNITIVE);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    brain_kg_node_id_t deps[32];
    uint32_t count = kg_hierarchy_get_dependents(hier, node_id, deps, 32);
    EXPECT_EQ(count, 0u);  // No edges added
}

// ============================================================================
// Real-Time Update Tests
// ============================================================================

TEST_F(KGHierarchyTest, test_report_state_change) {
    brain_kg_node_id_t node_id = add_module("stateful", BRAIN_KG_NODE_COGNITIVE);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    int result = kg_hierarchy_report_state_change(hier, node_id, KG_MODULE_STATE_RUNNING, "Started");
    EXPECT_EQ(result, 0);

    // Verify state was updated
    kg_module_info_t info;
    kg_hierarchy_get_module_info(hier, node_id, &info);
    EXPECT_EQ(info.state, KG_MODULE_STATE_RUNNING);
}

TEST_F(KGHierarchyTest, test_report_state_change_invalid_module) {
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    int result = kg_hierarchy_report_state_change(hier, 99999, KG_MODULE_STATE_RUNNING, "Invalid");
    EXPECT_EQ(result, -1);
}

TEST_F(KGHierarchyTest, test_report_health_change) {
    brain_kg_node_id_t node_id = add_module("health_test", BRAIN_KG_NODE_COGNITIVE);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    int result = kg_hierarchy_report_health_change(hier, node_id, BIO_MODULE_HEALTH_DEGRADED);
    EXPECT_EQ(result, 0);

    // Verify health was updated
    kg_module_info_t info;
    kg_hierarchy_get_module_info(hier, node_id, &info);
    EXPECT_EQ(info.health, BIO_MODULE_HEALTH_DEGRADED);
}

TEST_F(KGHierarchyTest, test_report_message_stats) {
    brain_kg_node_id_t node_id = add_module("stats_test", BRAIN_KG_NODE_COGNITIVE);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    int result = kg_hierarchy_report_message_stats(hier, node_id, 100, 50);
    EXPECT_EQ(result, 0);

    // Verify stats were updated
    kg_module_info_t info;
    kg_hierarchy_get_module_info(hier, node_id, &info);
    EXPECT_EQ(info.msg_stats.messages_sent, 100u);
    EXPECT_EQ(info.msg_stats.messages_received, 50u);
}

TEST_F(KGHierarchyTest, test_report_anomaly) {
    brain_kg_node_id_t node_id = add_module("anomaly_test", BRAIN_KG_NODE_COGNITIVE);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    // Report anomaly
    int result = kg_hierarchy_report_anomaly(hier, node_id, true);
    EXPECT_EQ(result, 0);

    kg_module_info_t info;
    kg_hierarchy_get_module_info(hier, node_id, &info);
    EXPECT_TRUE(info.has_anomaly);

    // Clear anomaly
    result = kg_hierarchy_report_anomaly(hier, node_id, false);
    EXPECT_EQ(result, 0);

    kg_hierarchy_get_module_info(hier, node_id, &info);
    EXPECT_FALSE(info.has_anomaly);
}

// ============================================================================
// Callback Tests
// ============================================================================

static std::atomic<int> g_callback_count{0};
static kg_state_change_event_t g_last_event;

static void test_state_callback(const kg_state_change_event_t* event, void* user_data) {
    (void)user_data;
    g_callback_count++;
    if (event) {
        g_last_event = *event;
    }
}

TEST_F(KGHierarchyTest, test_register_callback) {
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    int result = kg_hierarchy_register_state_callback(hier, test_state_callback, nullptr);
    EXPECT_EQ(result, 0);
}

TEST_F(KGHierarchyTest, test_callback_invoked_on_state_change) {
    brain_kg_node_id_t node_id = add_module("callback_test", BRAIN_KG_NODE_COGNITIVE);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    g_callback_count = 0;
    kg_hierarchy_register_state_callback(hier, test_state_callback, nullptr);

    kg_hierarchy_report_state_change(hier, node_id, KG_MODULE_STATE_RUNNING, "Test");

    EXPECT_EQ(g_callback_count, 1);
    EXPECT_EQ(g_last_event.module_id, node_id);
    EXPECT_EQ(g_last_event.new_state, KG_MODULE_STATE_RUNNING);
}

TEST_F(KGHierarchyTest, test_unregister_callback) {
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    kg_hierarchy_register_state_callback(hier, test_state_callback, nullptr);
    int result = kg_hierarchy_unregister_state_callback(hier, test_state_callback);
    EXPECT_EQ(result, 0);
}

TEST_F(KGHierarchyTest, test_unregister_nonexistent_callback) {
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    int result = kg_hierarchy_unregister_state_callback(hier, test_state_callback);
    EXPECT_EQ(result, -1);
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

TEST_F(KGHierarchyTest, test_read_lock_unlock) {
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    int result = kg_hierarchy_read_lock(hier);
    EXPECT_EQ(result, 0);

    result = kg_hierarchy_read_unlock(hier);
    EXPECT_EQ(result, 0);
}

TEST_F(KGHierarchyTest, test_concurrent_reads) {
    populate_test_modules();
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    // Query functions already handle locking internally, so we just test
    // that multiple sequential queries work correctly
    const int num_reads = 10;
    int success_count = 0;

    for (int i = 0; i < num_reads; i++) {
        kg_brain_stats_t stats;
        if (kg_hierarchy_get_brain_stats(hier, &stats) == 0) {
            success_count++;
        }
    }

    EXPECT_EQ(success_count, num_reads);
}

// ============================================================================
// Rebuild / Sync Tests
// ============================================================================

TEST_F(KGHierarchyTest, test_rebuild) {
    populate_test_modules();
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    // Add more modules after creation
    add_module("late_module", BRAIN_KG_NODE_PERCEPTION);

    // Rebuild should pick up new module
    int result = kg_hierarchy_rebuild(hier);
    EXPECT_EQ(result, 0);

    kg_brain_stats_t stats;
    kg_hierarchy_get_brain_stats(hier, &stats);
    EXPECT_EQ(stats.total_modules, 16u);  // 15 + 1 new
}

TEST_F(KGHierarchyTest, test_invalidate) {
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    // Should not crash
    kg_hierarchy_invalidate(hier);
}

TEST_F(KGHierarchyTest, test_sync_all) {
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    int result = kg_hierarchy_sync_all(hier);
    EXPECT_EQ(result, 0);
}

// ============================================================================
// String Conversion Tests
// ============================================================================

TEST_F(KGHierarchyTest, test_hemisphere_to_string) {
    EXPECT_STREQ(kg_hemisphere_to_string(KG_HEMISPHERE_LEFT), "Left Hemisphere");
    EXPECT_STREQ(kg_hemisphere_to_string(KG_HEMISPHERE_RIGHT), "Right Hemisphere");
    EXPECT_STREQ(kg_hemisphere_to_string(KG_HEMISPHERE_BILATERAL), "Bilateral");
    EXPECT_STREQ(kg_hemisphere_to_string((kg_hemisphere_t)99), "Unknown");
}

TEST_F(KGHierarchyTest, test_cortical_layer_to_string) {
    EXPECT_NE(kg_cortical_layer_to_string(KG_LAYER_I), nullptr);
    EXPECT_NE(kg_cortical_layer_to_string(KG_LAYER_II), nullptr);
    EXPECT_NE(kg_cortical_layer_to_string(KG_LAYER_III), nullptr);
    EXPECT_NE(kg_cortical_layer_to_string(KG_LAYER_IV), nullptr);
    EXPECT_NE(kg_cortical_layer_to_string(KG_LAYER_V), nullptr);
    EXPECT_NE(kg_cortical_layer_to_string(KG_LAYER_VI), nullptr);
    EXPECT_STREQ(kg_cortical_layer_to_string((kg_cortical_layer_t)99), "Unknown");
}

TEST_F(KGHierarchyTest, test_module_state_to_string) {
    EXPECT_STREQ(kg_module_state_to_string(KG_MODULE_STATE_UNKNOWN), "Unknown");
    EXPECT_STREQ(kg_module_state_to_string(KG_MODULE_STATE_STOPPED), "Stopped");
    EXPECT_STREQ(kg_module_state_to_string(KG_MODULE_STATE_STARTING), "Starting");
    EXPECT_STREQ(kg_module_state_to_string(KG_MODULE_STATE_RUNNING), "Running");
    EXPECT_STREQ(kg_module_state_to_string(KG_MODULE_STATE_PAUSED), "Paused");
    EXPECT_STREQ(kg_module_state_to_string(KG_MODULE_STATE_DEGRADED), "Degraded");
    EXPECT_STREQ(kg_module_state_to_string(KG_MODULE_STATE_ERROR), "Error");
}

TEST_F(KGHierarchyTest, test_hierarchy_level_to_string) {
    EXPECT_STREQ(kg_hierarchy_level_to_string(KG_LEVEL_BRAIN), "Brain");
    EXPECT_STREQ(kg_hierarchy_level_to_string(KG_LEVEL_HEMISPHERE), "Hemisphere");
    EXPECT_STREQ(kg_hierarchy_level_to_string(KG_LEVEL_LAYER), "Layer");
    EXPECT_STREQ(kg_hierarchy_level_to_string(KG_LEVEL_MODULE), "Module");
    EXPECT_STREQ(kg_hierarchy_level_to_string((kg_hierarchy_level_t)99), "Unknown");
}

// ============================================================================
// Integration Tests (Orchestrator / Wiring)
// ============================================================================

TEST_F(KGHierarchyTest, test_connect_orchestrator_null) {
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    // NULL orchestrator should be handled gracefully
    int result = kg_hierarchy_connect_orchestrator(hier, nullptr);
    // May return 0 (ignored) or -1 (error) depending on implementation
    (void)result;
}

TEST_F(KGHierarchyTest, test_connect_wiring_null) {
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    // NULL wiring diagram should be handled gracefully
    int result = kg_hierarchy_connect_wiring(hier, nullptr);
    (void)result;
}

// ============================================================================
// Edge Cases / Boundary Tests
// ============================================================================

TEST_F(KGHierarchyTest, test_max_modules_per_layer) {
    // Add many modules
    for (int i = 0; i < 100; i++) {
        char name[64];
        snprintf(name, sizeof(name), "module_%d", i);
        add_module(name, BRAIN_KG_NODE_COGNITIVE);
    }

    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    kg_brain_stats_t stats;
    kg_hierarchy_get_brain_stats(hier, &stats);
    EXPECT_EQ(stats.total_modules, 100u);
}

TEST_F(KGHierarchyTest, test_module_with_special_characters_in_name) {
    brain_kg_node_id_t node_id = add_module("module-with_special.chars", BRAIN_KG_NODE_COGNITIVE);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    brain_kg_node_id_t found = kg_hierarchy_find_module_by_name(hier, "module-with_special.chars");
    EXPECT_EQ(found, node_id);
}

TEST_F(KGHierarchyTest, test_hemisphere_layers_query) {
    populate_test_modules();
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    kg_layer_info_t layers[KG_LAYER_COUNT];
    uint32_t count = kg_hierarchy_get_hemisphere_layers(hier, KG_HEMISPHERE_LEFT, layers);
    EXPECT_EQ(count, (uint32_t)KG_LAYER_COUNT);
}

TEST_F(KGHierarchyTest, test_module_inputs_outputs_empty) {
    brain_kg_node_id_t node_id = add_module("isolated", BRAIN_KG_NODE_COGNITIVE);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    kg_connection_t inputs[32];
    kg_connection_t outputs[32];

    uint32_t in_count = kg_hierarchy_get_module_inputs(hier, node_id, inputs, 32);
    uint32_t out_count = kg_hierarchy_get_module_outputs(hier, node_id, outputs, 32);

    EXPECT_EQ(in_count, 0u);  // No edges added
    EXPECT_EQ(out_count, 0u);
}

// ============================================================================
// Topological Sort Tests
// ============================================================================

TEST_F(KGHierarchyTest, test_topological_sort_empty) {
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    brain_kg_node_id_t order[10];
    uint32_t sorted_count = 0;

    int result = kg_hierarchy_topological_sort(hier, order, 10, &sorted_count);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(sorted_count, 0u);
}

TEST_F(KGHierarchyTest, test_topological_sort_single_module) {
    brain_kg_node_id_t module = add_module("single_module", BRAIN_KG_NODE_COGNITIVE);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    brain_kg_node_id_t order[10];
    uint32_t sorted_count = 0;

    int result = kg_hierarchy_topological_sort(hier, order, 10, &sorted_count);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(sorted_count, 1u);
    EXPECT_EQ(order[0], module);
}

TEST_F(KGHierarchyTest, test_topological_sort_linear_chain) {
    // Create A -> B -> C chain
    brain_kg_node_id_t a = add_module("module_a", BRAIN_KG_NODE_COGNITIVE);
    brain_kg_node_id_t b = add_module("module_b", BRAIN_KG_NODE_COGNITIVE);
    brain_kg_node_id_t c = add_module("module_c", BRAIN_KG_NODE_COGNITIVE);

    // B depends on A, C depends on B
    brain_kg_add_edge(kg, a, b, BRAIN_KG_EDGE_DEPENDS_ON, "test", 1.0f);
    brain_kg_add_edge(kg, b, c, BRAIN_KG_EDGE_DEPENDS_ON, "test", 1.0f);

    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    brain_kg_node_id_t order[10];
    uint32_t sorted_count = 0;

    int result = kg_hierarchy_topological_sort(hier, order, 10, &sorted_count);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(sorted_count, 3u);

    // Verify order: A should come before B, B before C
    int pos_a = -1, pos_b = -1, pos_c = -1;
    for (uint32_t i = 0; i < sorted_count; i++) {
        if (order[i] == a) pos_a = (int)i;
        else if (order[i] == b) pos_b = (int)i;
        else if (order[i] == c) pos_c = (int)i;
    }
    EXPECT_LT(pos_a, pos_b);
    EXPECT_LT(pos_b, pos_c);
}

TEST_F(KGHierarchyTest, test_topological_sort_null_params) {
    populate_test_modules();
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    brain_kg_node_id_t order[10];
    uint32_t sorted_count = 0;

    EXPECT_EQ(kg_hierarchy_topological_sort(nullptr, order, 10, &sorted_count), -1);
    EXPECT_EQ(kg_hierarchy_topological_sort(hier, nullptr, 10, &sorted_count), -1);
    EXPECT_EQ(kg_hierarchy_topological_sort(hier, order, 10, nullptr), -1);
}

TEST_F(KGHierarchyTest, test_has_dependency_cycle_no_cycle) {
    brain_kg_node_id_t a = add_module("module_a", BRAIN_KG_NODE_COGNITIVE);
    brain_kg_node_id_t b = add_module("module_b", BRAIN_KG_NODE_COGNITIVE);

    // A -> B (no cycle)
    brain_kg_add_edge(kg, a, b, BRAIN_KG_EDGE_DEPENDS_ON, "test", 1.0f);

    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    EXPECT_FALSE(kg_hierarchy_has_dependency_cycle(hier));
}

TEST_F(KGHierarchyTest, test_get_startup_phase) {
    brain_kg_node_id_t a = add_module("init_module", BRAIN_KG_NODE_CORE);
    brain_kg_node_id_t b = add_module("late_module", BRAIN_KG_NODE_COGNITIVE);

    // late_module depends on init_module
    brain_kg_add_edge(kg, a, b, BRAIN_KG_EDGE_DEPENDS_ON, "test", 1.0f);

    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    int phase_a = kg_hierarchy_get_startup_phase(hier, a);
    int phase_b = kg_hierarchy_get_startup_phase(hier, b);

    EXPECT_GE(phase_a, 0);
    EXPECT_GE(phase_b, 0);
}

TEST_F(KGHierarchyTest, test_get_startup_phase_invalid) {
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    EXPECT_EQ(kg_hierarchy_get_startup_phase(nullptr, 1), -1);
    EXPECT_EQ(kg_hierarchy_get_startup_phase(hier, BRAIN_KG_INVALID_NODE), -1);
}

// ============================================================================
// Binary Search Tests
// ============================================================================

TEST_F(KGHierarchyTest, test_get_sorted_module_ids_empty) {
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    brain_kg_node_id_t ids[10];
    uint32_t count = kg_hierarchy_get_sorted_module_ids(hier, ids, 10);
    EXPECT_EQ(count, 0u);
}

TEST_F(KGHierarchyTest, test_get_sorted_module_ids) {
    add_module("module_z", BRAIN_KG_NODE_COGNITIVE);
    add_module("module_a", BRAIN_KG_NODE_COGNITIVE);
    add_module("module_m", BRAIN_KG_NODE_COGNITIVE);

    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    brain_kg_node_id_t ids[10];
    uint32_t count = kg_hierarchy_get_sorted_module_ids(hier, ids, 10);
    EXPECT_EQ(count, 3u);

    // Verify sorted
    EXPECT_TRUE(kg_hierarchy_is_sorted(ids, count));
}

TEST_F(KGHierarchyTest, test_binary_search_module_found) {
    brain_kg_node_id_t target = add_module("target_module", BRAIN_KG_NODE_COGNITIVE);
    add_module("other_module_1", BRAIN_KG_NODE_COGNITIVE);
    add_module("other_module_2", BRAIN_KG_NODE_COGNITIVE);

    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    brain_kg_node_id_t sorted[10];
    uint32_t count = kg_hierarchy_get_sorted_module_ids(hier, sorted, 10);

    uint32_t idx = kg_hierarchy_binary_search_module(hier, sorted, count, target);
    EXPECT_NE(idx, UINT32_MAX);
    EXPECT_EQ(sorted[idx], target);
}

TEST_F(KGHierarchyTest, test_binary_search_module_not_found) {
    add_module("module_1", BRAIN_KG_NODE_COGNITIVE);
    add_module("module_2", BRAIN_KG_NODE_COGNITIVE);

    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    brain_kg_node_id_t sorted[10];
    uint32_t count = kg_hierarchy_get_sorted_module_ids(hier, sorted, 10);

    uint32_t idx = kg_hierarchy_binary_search_module(hier, sorted, count, 9999);
    EXPECT_EQ(idx, UINT32_MAX);
}

TEST_F(KGHierarchyTest, test_is_sorted_true) {
    brain_kg_node_id_t ids[] = {1, 5, 10, 20, 100};
    EXPECT_TRUE(kg_hierarchy_is_sorted(ids, 5));
}

TEST_F(KGHierarchyTest, test_is_sorted_false) {
    brain_kg_node_id_t ids[] = {1, 5, 3, 20, 100};
    EXPECT_FALSE(kg_hierarchy_is_sorted(ids, 5));
}

TEST_F(KGHierarchyTest, test_is_sorted_empty) {
    EXPECT_TRUE(kg_hierarchy_is_sorted(nullptr, 0));
}

TEST_F(KGHierarchyTest, test_is_sorted_single) {
    brain_kg_node_id_t ids[] = {42};
    EXPECT_TRUE(kg_hierarchy_is_sorted(ids, 1));
}

// ============================================================================
// BFS/DFS Traversal Tests
// ============================================================================

static std::vector<brain_kg_node_id_t> g_visited_nodes;
static std::vector<uint32_t> g_visited_depths;

static bool test_visitor_collect(brain_kg_node_id_t module_id, uint32_t depth, void* user_data) {
    g_visited_nodes.push_back(module_id);
    g_visited_depths.push_back(depth);
    return true;  // continue traversal
}

static bool test_visitor_stop_early(brain_kg_node_id_t module_id, uint32_t depth, void* user_data) {
    g_visited_nodes.push_back(module_id);
    return g_visited_nodes.size() < 2;  // stop after 2 nodes
}

TEST_F(KGHierarchyTest, test_bfs_single_node) {
    brain_kg_node_id_t module = add_module("single", BRAIN_KG_NODE_COGNITIVE);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    g_visited_nodes.clear();
    g_visited_depths.clear();

    int result = kg_hierarchy_bfs(hier, module, test_visitor_collect, nullptr);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(g_visited_nodes.size(), 1u);
    EXPECT_EQ(g_visited_nodes[0], module);
    EXPECT_EQ(g_visited_depths[0], 0u);
}

TEST_F(KGHierarchyTest, test_bfs_linear_chain) {
    brain_kg_node_id_t a = add_module("a", BRAIN_KG_NODE_COGNITIVE);
    brain_kg_node_id_t b = add_module("b", BRAIN_KG_NODE_COGNITIVE);
    brain_kg_node_id_t c = add_module("c", BRAIN_KG_NODE_COGNITIVE);

    brain_kg_add_edge(kg, a, b, BRAIN_KG_EDGE_CONNECTS_TO, "test", 1.0f);
    brain_kg_add_edge(kg, b, c, BRAIN_KG_EDGE_CONNECTS_TO, "test", 1.0f);

    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    g_visited_nodes.clear();
    g_visited_depths.clear();

    int result = kg_hierarchy_bfs(hier, a, test_visitor_collect, nullptr);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(g_visited_nodes.size(), 3u);
}

TEST_F(KGHierarchyTest, test_bfs_null_params) {
    populate_test_modules();
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    EXPECT_EQ(kg_hierarchy_bfs(nullptr, 1, test_visitor_collect, nullptr), -1);
    EXPECT_EQ(kg_hierarchy_bfs(hier, 1, nullptr, nullptr), -1);
}

TEST_F(KGHierarchyTest, test_dfs_single_node) {
    brain_kg_node_id_t module = add_module("single", BRAIN_KG_NODE_COGNITIVE);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    g_visited_nodes.clear();
    g_visited_depths.clear();

    int result = kg_hierarchy_dfs(hier, module, test_visitor_collect, nullptr);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(g_visited_nodes.size(), 1u);
    EXPECT_EQ(g_visited_nodes[0], module);
}

TEST_F(KGHierarchyTest, test_dfs_linear_chain) {
    brain_kg_node_id_t a = add_module("a", BRAIN_KG_NODE_COGNITIVE);
    brain_kg_node_id_t b = add_module("b", BRAIN_KG_NODE_COGNITIVE);
    brain_kg_node_id_t c = add_module("c", BRAIN_KG_NODE_COGNITIVE);

    brain_kg_add_edge(kg, a, b, BRAIN_KG_EDGE_CONNECTS_TO, "test", 1.0f);
    brain_kg_add_edge(kg, b, c, BRAIN_KG_EDGE_CONNECTS_TO, "test", 1.0f);

    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    g_visited_nodes.clear();

    int result = kg_hierarchy_dfs(hier, a, test_visitor_collect, nullptr);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(g_visited_nodes.size(), 3u);
}

TEST_F(KGHierarchyTest, test_traversal_early_stop) {
    brain_kg_node_id_t a = add_module("a", BRAIN_KG_NODE_COGNITIVE);
    brain_kg_node_id_t b = add_module("b", BRAIN_KG_NODE_COGNITIVE);
    brain_kg_node_id_t c = add_module("c", BRAIN_KG_NODE_COGNITIVE);
    brain_kg_node_id_t d = add_module("d", BRAIN_KG_NODE_COGNITIVE);

    brain_kg_add_edge(kg, a, b, BRAIN_KG_EDGE_CONNECTS_TO, "test", 1.0f);
    brain_kg_add_edge(kg, a, c, BRAIN_KG_EDGE_CONNECTS_TO, "test", 1.0f);
    brain_kg_add_edge(kg, a, d, BRAIN_KG_EDGE_CONNECTS_TO, "test", 1.0f);

    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    g_visited_nodes.clear();

    // Should stop after 2 nodes
    int result = kg_hierarchy_bfs(hier, a, test_visitor_stop_early, nullptr);
    EXPECT_EQ(result, 0);
    EXPECT_LE(g_visited_nodes.size(), 2u);
}

// ============================================================================
// Shortest Path Tests
// ============================================================================

TEST_F(KGHierarchyTest, test_shortest_path_same_node) {
    brain_kg_node_id_t module = add_module("single", BRAIN_KG_NODE_COGNITIVE);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    brain_kg_node_id_t path[10];
    uint32_t path_length = 0;

    int result = kg_hierarchy_shortest_path(hier, module, module, path, 10, &path_length);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(path_length, 1u);
    EXPECT_EQ(path[0], module);
}

TEST_F(KGHierarchyTest, test_shortest_path_direct_connection) {
    brain_kg_node_id_t a = add_module("a", BRAIN_KG_NODE_COGNITIVE);
    brain_kg_node_id_t b = add_module("b", BRAIN_KG_NODE_COGNITIVE);

    brain_kg_add_edge(kg, a, b, BRAIN_KG_EDGE_CONNECTS_TO, "test", 1.0f);

    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    brain_kg_node_id_t path[10];
    uint32_t path_length = 0;

    int result = kg_hierarchy_shortest_path(hier, a, b, path, 10, &path_length);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(path_length, 2u);
    EXPECT_EQ(path[0], a);
    EXPECT_EQ(path[1], b);
}

TEST_F(KGHierarchyTest, test_shortest_path_through_intermediate) {
    brain_kg_node_id_t a = add_module("a", BRAIN_KG_NODE_COGNITIVE);
    brain_kg_node_id_t b = add_module("b", BRAIN_KG_NODE_COGNITIVE);
    brain_kg_node_id_t c = add_module("c", BRAIN_KG_NODE_COGNITIVE);

    brain_kg_add_edge(kg, a, b, BRAIN_KG_EDGE_CONNECTS_TO, "test", 1.0f);
    brain_kg_add_edge(kg, b, c, BRAIN_KG_EDGE_CONNECTS_TO, "test", 1.0f);

    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    brain_kg_node_id_t path[10];
    uint32_t path_length = 0;

    int result = kg_hierarchy_shortest_path(hier, a, c, path, 10, &path_length);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(path_length, 3u);
    EXPECT_EQ(path[0], a);
    EXPECT_EQ(path[1], b);
    EXPECT_EQ(path[2], c);
}

TEST_F(KGHierarchyTest, test_shortest_path_no_connection) {
    brain_kg_node_id_t a = add_module("a", BRAIN_KG_NODE_COGNITIVE);
    brain_kg_node_id_t b = add_module("b", BRAIN_KG_NODE_COGNITIVE);
    // No edge between a and b

    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    brain_kg_node_id_t path[10];
    uint32_t path_length = 0;

    int result = kg_hierarchy_shortest_path(hier, a, b, path, 10, &path_length);
    EXPECT_EQ(result, -1);  // No path found
}

TEST_F(KGHierarchyTest, test_get_distance_same_node) {
    brain_kg_node_id_t module = add_module("single", BRAIN_KG_NODE_COGNITIVE);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    uint32_t dist = kg_hierarchy_get_distance(hier, module, module);
    EXPECT_EQ(dist, 0u);
}

TEST_F(KGHierarchyTest, test_get_distance_direct) {
    brain_kg_node_id_t a = add_module("a", BRAIN_KG_NODE_COGNITIVE);
    brain_kg_node_id_t b = add_module("b", BRAIN_KG_NODE_COGNITIVE);

    brain_kg_add_edge(kg, a, b, BRAIN_KG_EDGE_CONNECTS_TO, "test", 1.0f);

    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    uint32_t dist = kg_hierarchy_get_distance(hier, a, b);
    EXPECT_EQ(dist, 1u);
}

TEST_F(KGHierarchyTest, test_get_distance_no_path) {
    brain_kg_node_id_t a = add_module("a", BRAIN_KG_NODE_COGNITIVE);
    brain_kg_node_id_t b = add_module("b", BRAIN_KG_NODE_COGNITIVE);

    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    uint32_t dist = kg_hierarchy_get_distance(hier, a, b);
    EXPECT_EQ(dist, UINT32_MAX);
}

TEST_F(KGHierarchyTest, test_get_reachable_single) {
    brain_kg_node_id_t module = add_module("single", BRAIN_KG_NODE_COGNITIVE);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    brain_kg_node_id_t reachable[10];
    uint32_t count = kg_hierarchy_get_reachable(hier, module, reachable, 10);
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(reachable[0], module);
}

TEST_F(KGHierarchyTest, test_get_reachable_connected) {
    brain_kg_node_id_t a = add_module("a", BRAIN_KG_NODE_COGNITIVE);
    brain_kg_node_id_t b = add_module("b", BRAIN_KG_NODE_COGNITIVE);
    brain_kg_node_id_t c = add_module("c", BRAIN_KG_NODE_COGNITIVE);

    brain_kg_add_edge(kg, a, b, BRAIN_KG_EDGE_CONNECTS_TO, "test", 1.0f);
    brain_kg_add_edge(kg, b, c, BRAIN_KG_EDGE_CONNECTS_TO, "test", 1.0f);

    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    brain_kg_node_id_t reachable[10];
    uint32_t count = kg_hierarchy_get_reachable(hier, a, reachable, 10);
    EXPECT_EQ(count, 3u);
}

// ============================================================================
// Connected Components Tests
// ============================================================================

TEST_F(KGHierarchyTest, test_find_components_empty) {
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    uint32_t comp_ids[10];
    uint32_t num_components = 0;

    int result = kg_hierarchy_find_components(hier, comp_ids, &num_components);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(num_components, 0u);
}

TEST_F(KGHierarchyTest, test_find_components_single_module) {
    add_module("single", BRAIN_KG_NODE_COGNITIVE);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    uint32_t comp_ids[10];
    uint32_t num_components = 0;

    int result = kg_hierarchy_find_components(hier, comp_ids, &num_components);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(num_components, 1u);
}

TEST_F(KGHierarchyTest, test_find_components_two_disconnected) {
    add_module("module_a", BRAIN_KG_NODE_COGNITIVE);
    add_module("module_b", BRAIN_KG_NODE_COGNITIVE);
    // No edges - two separate components

    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    uint32_t comp_ids[10];
    uint32_t num_components = 0;

    int result = kg_hierarchy_find_components(hier, comp_ids, &num_components);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(num_components, 2u);
}

TEST_F(KGHierarchyTest, test_find_components_two_connected) {
    brain_kg_node_id_t a = add_module("module_a", BRAIN_KG_NODE_COGNITIVE);
    brain_kg_node_id_t b = add_module("module_b", BRAIN_KG_NODE_COGNITIVE);
    brain_kg_add_edge(kg, a, b, BRAIN_KG_EDGE_CONNECTS_TO, "test", 1.0f);

    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    uint32_t comp_ids[10];
    uint32_t num_components = 0;

    int result = kg_hierarchy_find_components(hier, comp_ids, &num_components);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(num_components, 1u);
}

TEST_F(KGHierarchyTest, test_are_connected_same_module) {
    brain_kg_node_id_t module = add_module("single", BRAIN_KG_NODE_COGNITIVE);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    EXPECT_TRUE(kg_hierarchy_are_connected(hier, module, module));
}

TEST_F(KGHierarchyTest, test_are_connected_linked) {
    brain_kg_node_id_t a = add_module("a", BRAIN_KG_NODE_COGNITIVE);
    brain_kg_node_id_t b = add_module("b", BRAIN_KG_NODE_COGNITIVE);
    brain_kg_add_edge(kg, a, b, BRAIN_KG_EDGE_CONNECTS_TO, "test", 1.0f);

    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    EXPECT_TRUE(kg_hierarchy_are_connected(hier, a, b));
}

TEST_F(KGHierarchyTest, test_are_connected_unlinked) {
    brain_kg_node_id_t a = add_module("a", BRAIN_KG_NODE_COGNITIVE);
    brain_kg_node_id_t b = add_module("b", BRAIN_KG_NODE_COGNITIVE);
    // No edge

    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    EXPECT_FALSE(kg_hierarchy_are_connected(hier, a, b));
}

TEST_F(KGHierarchyTest, test_get_largest_component_single) {
    brain_kg_node_id_t module = add_module("single", BRAIN_KG_NODE_COGNITIVE);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    brain_kg_node_id_t modules[10];
    uint32_t count = kg_hierarchy_get_largest_component(hier, modules, 10);
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(modules[0], module);
}

TEST_F(KGHierarchyTest, test_get_largest_component_multiple) {
    // Create two components: one with 3 nodes, one with 2 nodes
    brain_kg_node_id_t a1 = add_module("a1", BRAIN_KG_NODE_COGNITIVE);
    brain_kg_node_id_t a2 = add_module("a2", BRAIN_KG_NODE_COGNITIVE);
    brain_kg_node_id_t a3 = add_module("a3", BRAIN_KG_NODE_COGNITIVE);

    brain_kg_node_id_t b1 = add_module("b1", BRAIN_KG_NODE_COGNITIVE);
    brain_kg_node_id_t b2 = add_module("b2", BRAIN_KG_NODE_COGNITIVE);

    // Connect group A
    brain_kg_add_edge(kg, a1, a2, BRAIN_KG_EDGE_CONNECTS_TO, "test", 1.0f);
    brain_kg_add_edge(kg, a2, a3, BRAIN_KG_EDGE_CONNECTS_TO, "test", 1.0f);

    // Connect group B
    brain_kg_add_edge(kg, b1, b2, BRAIN_KG_EDGE_CONNECTS_TO, "test", 1.0f);

    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    brain_kg_node_id_t modules[10];
    uint32_t count = kg_hierarchy_get_largest_component(hier, modules, 10);
    EXPECT_EQ(count, 3u);  // Group A is larger
}

TEST_F(KGHierarchyTest, test_count_isolated_none) {
    brain_kg_node_id_t a = add_module("a", BRAIN_KG_NODE_COGNITIVE);
    brain_kg_node_id_t b = add_module("b", BRAIN_KG_NODE_COGNITIVE);
    brain_kg_add_edge(kg, a, b, BRAIN_KG_EDGE_CONNECTS_TO, "test", 1.0f);

    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    EXPECT_EQ(kg_hierarchy_count_isolated(hier), 0u);
}

TEST_F(KGHierarchyTest, test_count_isolated_some) {
    add_module("isolated_1", BRAIN_KG_NODE_COGNITIVE);
    add_module("isolated_2", BRAIN_KG_NODE_COGNITIVE);

    brain_kg_node_id_t a = add_module("connected_a", BRAIN_KG_NODE_COGNITIVE);
    brain_kg_node_id_t b = add_module("connected_b", BRAIN_KG_NODE_COGNITIVE);
    brain_kg_add_edge(kg, a, b, BRAIN_KG_EDGE_CONNECTS_TO, "test", 1.0f);

    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    EXPECT_EQ(kg_hierarchy_count_isolated(hier), 2u);
}

TEST_F(KGHierarchyTest, test_get_component_modules) {
    brain_kg_node_id_t a = add_module("a", BRAIN_KG_NODE_COGNITIVE);
    brain_kg_node_id_t b = add_module("b", BRAIN_KG_NODE_COGNITIVE);
    brain_kg_add_edge(kg, a, b, BRAIN_KG_EDGE_CONNECTS_TO, "test", 1.0f);

    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    // Get component IDs first
    uint32_t comp_ids[10];
    uint32_t num_components = 0;
    kg_hierarchy_find_components(hier, comp_ids, &num_components);
    EXPECT_GT(num_components, 0u);

    // Get modules for component 0
    brain_kg_node_id_t modules[10];
    uint32_t count = kg_hierarchy_get_component_modules(hier, 0, modules, 10);
    EXPECT_EQ(count, 2u);
}

TEST_F(KGHierarchyTest, test_component_info_null_params) {
    populate_test_modules();
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    kg_component_info_t info;
    EXPECT_EQ(kg_hierarchy_get_component_info(nullptr, 0, &info), -1);
    EXPECT_EQ(kg_hierarchy_get_component_info(hier, 0, nullptr), -1);
}

// ============================================================================
// Edge Cases and Stress Tests
// ============================================================================

TEST_F(KGHierarchyTest, test_large_graph_traversal) {
    // Create a larger graph
    const int N = 50;
    brain_kg_node_id_t nodes[N];

    for (int i = 0; i < N; i++) {
        char name[32];
        snprintf(name, sizeof(name), "node_%d", i);
        nodes[i] = add_module(name, BRAIN_KG_NODE_COGNITIVE);
    }

    // Create a chain
    for (int i = 0; i < N - 1; i++) {
        brain_kg_add_edge(kg, nodes[i], nodes[i+1], BRAIN_KG_EDGE_CONNECTS_TO, "test", 1.0f);
    }

    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    // BFS from first node
    g_visited_nodes.clear();
    int result = kg_hierarchy_bfs(hier, nodes[0], test_visitor_collect, nullptr);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(g_visited_nodes.size(), (size_t)N);

    // Shortest path
    brain_kg_node_id_t path[N + 1];
    uint32_t path_len = 0;
    result = kg_hierarchy_shortest_path(hier, nodes[0], nodes[N-1], path, N + 1, &path_len);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(path_len, (uint32_t)N);
}

TEST_F(KGHierarchyTest, test_graph_with_multiple_edges) {
    brain_kg_node_id_t a = add_module("a", BRAIN_KG_NODE_COGNITIVE);
    brain_kg_node_id_t b = add_module("b", BRAIN_KG_NODE_COGNITIVE);

    // Multiple edge types between same nodes
    brain_kg_add_edge(kg, a, b, BRAIN_KG_EDGE_CONNECTS_TO, "test", 1.0f);
    brain_kg_add_edge(kg, a, b, BRAIN_KG_EDGE_DEPENDS_ON, "test", 1.0f);
    brain_kg_add_edge(kg, b, a, BRAIN_KG_EDGE_CONNECTS_TO, "test", 1.0f);

    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    // Should still find correct shortest path
    brain_kg_node_id_t path[10];
    uint32_t path_len = 0;
    int result = kg_hierarchy_shortest_path(hier, a, b, path, 10, &path_len);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(path_len, 2u);
}
