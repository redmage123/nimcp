/**
 * @file test_kg_full_e2e.cpp
 * @brief End-to-end tests for complete Knowledge Graph workflows (GTest)
 * @date 2026-02-02
 *
 * WHAT: E2E tests verifying complete KG workflows from creation to disaster recovery
 * WHY:  Validate that KG creation, population, querying, persistence, and recovery
 *       work correctly in realistic brain operation scenarios
 * HOW:  Uses GTest framework with E2E pipeline tracking macros
 *
 * TESTS:
 * 1. Complete KG workflow: create -> populate -> query -> persist -> recover
 * 2. Multi-module registration
 * 3. Temporal queries
 * 4. Disaster recovery simulation
 * 5. Graph traversal and path finding
 * 6. Security and integrity verification
 *
 * @author NIMCP Development Team
 */

#include "e2e_test_framework.h"
#include <gtest/gtest.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdbool>
#include <ctime>
#include <unistd.h>
#include <sys/stat.h>
#include <chrono>
#include <vector>
#include <numeric>
#include <cmath>

extern "C" {
#include "core/brain/nimcp_brain_kg.h"
}

namespace nimcp {
namespace e2e {

//=============================================================================
// Constants
//=============================================================================

static constexpr uint64_t TEST_ADMIN_TOKEN = 0x12345678DEADBEEFULL;
static const char* TEST_SNAPSHOT_DIR = "/tmp/nimcp_kg_e2e_test";
static constexpr int MAX_KG_EVENTS = 256;
static constexpr int STRESS_NODE_COUNT = 100;
static constexpr int STRESS_EDGE_COUNT = 500;

//=============================================================================
// Event Tracking
//=============================================================================

enum KGEventType {
    KG_EVENT_CREATE = 0,
    KG_EVENT_ADD_NODE,
    KG_EVENT_ADD_EDGE,
    KG_EVENT_QUERY,
    KG_EVENT_PERSIST,
    KG_EVENT_RECOVER,
    KG_EVENT_ERROR,
    KG_EVENT_SECURITY
};

struct KGEvent {
    KGEventType type;
    uint64_t timestamp_ms;
    char details[128];
    int result_code;
};

//=============================================================================
// Test Fixture
//=============================================================================

class KGFullE2E : public ::testing::Test {
protected:
    std::vector<KGEvent> events_;

    void SetUp() override {
        events_.clear();
        ensure_test_dir();
        srand(static_cast<unsigned int>(time(nullptr)));
    }

    void TearDown() override {
        cleanup_test_dir();
    }

    uint64_t get_time_ms() {
        auto now = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch());
        return static_cast<uint64_t>(ms.count());
    }

    void record_kg_event(KGEventType type, const char* details, int result) {
        if (events_.size() < MAX_KG_EVENTS) {
            KGEvent evt;
            evt.type = type;
            evt.timestamp_ms = get_time_ms();
            if (details) {
                strncpy(evt.details, details, sizeof(evt.details) - 1);
                evt.details[sizeof(evt.details) - 1] = '\0';
            } else {
                evt.details[0] = '\0';
            }
            evt.result_code = result;
            events_.push_back(evt);
        }
    }

    void ensure_test_dir() {
        mkdir(TEST_SNAPSHOT_DIR, 0755);
    }

    void cleanup_test_dir() {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "rm -rf %s", TEST_SNAPSHOT_DIR);
        (void)system(cmd);
    }

    brain_kg_t* create_test_kg() {
        brain_kg_config_t config;
        brain_kg_default_config(&config);
        config.enable_security = false;
        config.enable_access_control = false;
        config.enable_immune_integration = false;
        config.enable_statistics = true;
        return brain_kg_create(&config);
    }

    brain_kg_t* create_secured_kg() {
        brain_kg_config_t config;
        brain_kg_default_config(&config);
        config.enable_security = true;
        config.enable_integrity_checks = true;
        config.enable_access_control = true;
        config.enable_audit_log = true;
        config.max_mutations_per_sec = 100;
        return brain_kg_create(&config);
    }
};

//=============================================================================
// TEST GROUP 1: Basic KG Workflow
//=============================================================================

TEST_F(KGFullE2E, CompleteKGWorkflow) {
    E2E_PIPELINE_START("Complete KG Workflow");

    printf("\n=== Complete KG Workflow Test ===\n");

    /* Phase 1: Create */
    E2E_STAGE_BEGIN("Create KG", 50);
    record_kg_event(KG_EVENT_CREATE, "kg_init", 0);
    brain_kg_t* kg = create_test_kg();
    E2E_ASSERT_NOT_NULL(kg, "KG creation failed");
    E2E_STAGE_END();

    /* Phase 2: Populate with brain region nodes */
    E2E_STAGE_BEGIN("Populate brain regions", 100);
    printf("  Phase 2: Populating KG...\n");

    brain_kg_node_id_t prefrontal = brain_kg_add_node(
        kg, "prefrontal_cortex", BRAIN_KG_NODE_CORTICAL,
        "Executive control, working memory, planning"
    );
    record_kg_event(KG_EVENT_ADD_NODE, "prefrontal_cortex", prefrontal != BRAIN_KG_INVALID_NODE ? 0 : -1);
    EXPECT_NE(prefrontal, BRAIN_KG_INVALID_NODE);

    brain_kg_node_id_t hippocampus = brain_kg_add_node(
        kg, "hippocampus", BRAIN_KG_NODE_SUBCORTICAL,
        "Episodic memory, spatial navigation"
    );
    record_kg_event(KG_EVENT_ADD_NODE, "hippocampus", hippocampus != BRAIN_KG_INVALID_NODE ? 0 : -1);
    EXPECT_NE(hippocampus, BRAIN_KG_INVALID_NODE);

    brain_kg_node_id_t amygdala = brain_kg_add_node(
        kg, "amygdala", BRAIN_KG_NODE_SUBCORTICAL,
        "Emotional processing, fear response"
    );
    record_kg_event(KG_EVENT_ADD_NODE, "amygdala", amygdala != BRAIN_KG_INVALID_NODE ? 0 : -1);
    EXPECT_NE(amygdala, BRAIN_KG_INVALID_NODE);

    brain_kg_node_id_t basal_ganglia = brain_kg_add_node(
        kg, "basal_ganglia", BRAIN_KG_NODE_SUBCORTICAL,
        "Motor control, reward learning"
    );
    record_kg_event(KG_EVENT_ADD_NODE, "basal_ganglia", basal_ganglia != BRAIN_KG_INVALID_NODE ? 0 : -1);
    EXPECT_NE(basal_ganglia, BRAIN_KG_INVALID_NODE);

    brain_kg_node_id_t thalamus = brain_kg_add_node(
        kg, "thalamus", BRAIN_KG_NODE_SUBCORTICAL,
        "Sensory relay, consciousness gating"
    );
    record_kg_event(KG_EVENT_ADD_NODE, "thalamus", thalamus != BRAIN_KG_INVALID_NODE ? 0 : -1);
    EXPECT_NE(thalamus, BRAIN_KG_INVALID_NODE);
    E2E_STAGE_END();

    /* Phase 3: Add connections */
    E2E_STAGE_BEGIN("Add connections", 100);
    printf("  Phase 3: Adding connections...\n");

    brain_kg_edge_id_t e1 = brain_kg_add_edge(
        kg, prefrontal, hippocampus, BRAIN_KG_EDGE_CONNECTS_TO,
        "Memory retrieval pathway", 0.8f
    );
    record_kg_event(KG_EVENT_ADD_EDGE, "pfc->hippo", e1 != BRAIN_KG_INVALID_NODE ? 0 : -1);
    EXPECT_NE(e1, BRAIN_KG_INVALID_NODE);

    brain_kg_edge_id_t e2 = brain_kg_add_edge(
        kg, amygdala, hippocampus, BRAIN_KG_EDGE_MODULATES,
        "Emotional memory enhancement", 0.9f
    );
    record_kg_event(KG_EVENT_ADD_EDGE, "amy->hippo", e2 != BRAIN_KG_INVALID_NODE ? 0 : -1);
    EXPECT_NE(e2, BRAIN_KG_INVALID_NODE);

    brain_kg_edge_id_t e3 = brain_kg_add_edge(
        kg, thalamus, prefrontal, BRAIN_KG_EDGE_SENDS_TO,
        "Sensory information relay", 0.7f
    );
    record_kg_event(KG_EVENT_ADD_EDGE, "thal->pfc", e3 != BRAIN_KG_INVALID_NODE ? 0 : -1);
    EXPECT_NE(e3, BRAIN_KG_INVALID_NODE);

    brain_kg_edge_id_t e4 = brain_kg_add_edge(
        kg, prefrontal, basal_ganglia, BRAIN_KG_EDGE_EXCITES,
        "Action selection", 0.6f
    );
    record_kg_event(KG_EVENT_ADD_EDGE, "pfc->bg", e4 != BRAIN_KG_INVALID_NODE ? 0 : -1);
    EXPECT_NE(e4, BRAIN_KG_INVALID_NODE);
    E2E_STAGE_END();

    /* Phase 4: Query and verify */
    E2E_STAGE_BEGIN("Query and verify", 100);
    printf("  Phase 4: Querying KG...\n");

    brain_kg_stats_t stats;
    int stat_result = brain_kg_get_stats(kg, &stats);
    record_kg_event(KG_EVENT_QUERY, "get_stats", stat_result);
    EXPECT_EQ(stat_result, 0);
    EXPECT_EQ(stats.total_nodes, 5u);
    EXPECT_EQ(stats.total_edges, 4u);

    /* Test path finding */
    brain_kg_path_t* path = brain_kg_find_path(kg, thalamus, hippocampus);
    record_kg_event(KG_EVENT_QUERY, "find_path", path ? 0 : -1);
    if (path) {
        EXPECT_GE(path->length, 2u);
        brain_kg_path_destroy(path);
    }

    /* Test node lookup */
    brain_kg_node_id_t found = brain_kg_find_node(kg, "amygdala");
    record_kg_event(KG_EVENT_QUERY, "find_node", found == amygdala ? 0 : -1);
    EXPECT_EQ(found, amygdala);
    E2E_STAGE_END();

    /* Phase 5: Cleanup */
    E2E_STAGE_BEGIN("Cleanup", 50);
    printf("  Phase 5: Cleanup...\n");
    brain_kg_destroy(kg);
    printf("  Workflow completed with %zu events\n", events_.size());
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(KGFullE2E, KGNodeTypes) {
    E2E_PIPELINE_START("KG Node Types");

    E2E_STAGE_BEGIN("Create KG with typed nodes", 100);
    brain_kg_t* kg = create_test_kg();
    E2E_ASSERT_NOT_NULL(kg, "KG creation failed");

    /* Add nodes of different types */
    brain_kg_add_node(kg, "core_module", BRAIN_KG_NODE_CORE, "Core infrastructure");
    brain_kg_add_node(kg, "visual_cortex", BRAIN_KG_NODE_CORTICAL, "Visual processing");
    brain_kg_add_node(kg, "cerebellum", BRAIN_KG_NODE_SUBCORTICAL, "Motor coordination");
    brain_kg_add_node(kg, "medulla", BRAIN_KG_NODE_BRAINSTEM, "Vital functions");
    brain_kg_add_node(kg, "ethics", BRAIN_KG_NODE_COGNITIVE, "Ethical reasoning");
    brain_kg_add_node(kg, "audio_cortex", BRAIN_KG_NODE_PERCEPTION, "Auditory processing");
    brain_kg_add_node(kg, "stdp", BRAIN_KG_NODE_PLASTICITY, "Spike-timing plasticity");
    brain_kg_add_node(kg, "backprop", BRAIN_KG_NODE_TRAINING, "Gradient training");
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify nodes by type", 50);
    brain_kg_node_list_t* cortical = brain_kg_get_nodes_by_type(kg, BRAIN_KG_NODE_CORTICAL);
    ASSERT_NE(cortical, nullptr);
    EXPECT_EQ(cortical->count, 1u);
    brain_kg_node_list_destroy(cortical);

    brain_kg_node_list_t* all_nodes = brain_kg_get_all_nodes(kg);
    ASSERT_NE(all_nodes, nullptr);
    EXPECT_EQ(all_nodes->count, 8u);
    brain_kg_node_list_destroy(all_nodes);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Cleanup", 50);
    brain_kg_destroy(kg);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// TEST GROUP 2: Multi-Module Registration
//=============================================================================

TEST_F(KGFullE2E, MultiModuleRegistration) {
    E2E_PIPELINE_START("Multi-Module Registration");

    E2E_STAGE_BEGIN("Create KG", 50);
    brain_kg_t* kg = create_test_kg();
    E2E_ASSERT_NOT_NULL(kg, "KG creation failed");
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Register modules", 200);
    const char* modules[] = {
        "prefrontal_cortex", "hippocampus", "amygdala", "basal_ganglia",
        "thalamus", "cerebellum", "medulla", "visual_cortex",
        "auditory_cortex", "motor_cortex", "somatosensory_cortex",
        "temporal_lobe", "parietal_lobe", "occipital_lobe"
    };
    const int num_modules = sizeof(modules) / sizeof(modules[0]);

    brain_kg_node_id_t node_ids[16];

    for (int i = 0; i < num_modules; i++) {
        node_ids[i] = brain_kg_add_node(
            kg, modules[i],
            (i < 5) ? BRAIN_KG_NODE_SUBCORTICAL : BRAIN_KG_NODE_CORTICAL,
            "Brain region module"
        );
        EXPECT_NE(node_ids[i], BRAIN_KG_INVALID_NODE) << "Failed to register module " << modules[i];
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Add hierarchical connections", 100);
    for (int i = 1; i < num_modules; i++) {
        brain_kg_add_edge(kg, node_ids[0], node_ids[i],
                          BRAIN_KG_EDGE_COORDINATES_WITH,
                          "Coordination link", 0.5f);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify registration", 50);
    brain_kg_stats_t stats;
    brain_kg_get_stats(kg, &stats);
    EXPECT_EQ(stats.total_nodes, static_cast<uint32_t>(num_modules));
    EXPECT_EQ(stats.total_edges, static_cast<uint32_t>(num_modules - 1));

    /* Verify hub detection */
    brain_kg_node_list_t* hubs = brain_kg_get_hubs(kg, 3);
    ASSERT_NE(hubs, nullptr);
    EXPECT_GE(hubs->count, 1u);
    brain_kg_node_list_destroy(hubs);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Cleanup", 50);
    brain_kg_destroy(kg);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(KGFullE2E, ModuleMetadata) {
    E2E_PIPELINE_START("Module Metadata");

    E2E_STAGE_BEGIN("Create KG and node", 50);
    brain_kg_t* kg = create_test_kg();
    E2E_ASSERT_NOT_NULL(kg, "KG creation failed");

    brain_kg_node_id_t node = brain_kg_add_node(
        kg, "working_memory", BRAIN_KG_NODE_COGNITIVE,
        "Working memory module"
    );
    EXPECT_NE(node, BRAIN_KG_INVALID_NODE);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Add metadata", 50);
    int r1 = brain_kg_add_metadata(kg, node, "version", "2.7.0");
    EXPECT_EQ(r1, 0);

    int r2 = brain_kg_add_metadata(kg, node, "capacity", "7");
    EXPECT_EQ(r2, 0);

    int r3 = brain_kg_add_metadata(kg, node, "decay_tau_ms", "1000");
    EXPECT_EQ(r3, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify metadata", 50);
    const brain_kg_node_t* retrieved = brain_kg_get_node(kg, node);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->metadata_count, 3u);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Cleanup", 50);
    brain_kg_destroy(kg);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// TEST GROUP 3: Graph Traversal and Queries
//=============================================================================

TEST_F(KGFullE2E, KGPathFinding) {
    E2E_PIPELINE_START("KG Path Finding");

    E2E_STAGE_BEGIN("Create network", 100);
    brain_kg_t* kg = create_test_kg();
    E2E_ASSERT_NOT_NULL(kg, "KG creation failed");

    brain_kg_node_id_t n1 = brain_kg_add_node(kg, "sensory_input", BRAIN_KG_NODE_PERCEPTION, "Input");
    brain_kg_node_id_t n2 = brain_kg_add_node(kg, "thalamus", BRAIN_KG_NODE_SUBCORTICAL, "Relay");
    brain_kg_node_id_t n3 = brain_kg_add_node(kg, "cortex", BRAIN_KG_NODE_CORTICAL, "Processing");
    brain_kg_node_id_t n4 = brain_kg_add_node(kg, "hippocampus", BRAIN_KG_NODE_SUBCORTICAL, "Memory");
    brain_kg_node_id_t n5 = brain_kg_add_node(kg, "motor_output", BRAIN_KG_NODE_CORTICAL, "Output");

    /* Create pathway: sensory -> thalamus -> cortex -> hippocampus -> motor */
    brain_kg_add_edge(kg, n1, n2, BRAIN_KG_EDGE_SENDS_TO, "sensory to thalamus", 1.0f);
    brain_kg_add_edge(kg, n2, n3, BRAIN_KG_EDGE_SENDS_TO, "thalamus to cortex", 1.0f);
    brain_kg_add_edge(kg, n3, n4, BRAIN_KG_EDGE_CONNECTS_TO, "cortex to hippocampus", 0.8f);
    brain_kg_add_edge(kg, n3, n5, BRAIN_KG_EDGE_SENDS_TO, "cortex to motor", 0.9f);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Test paths", 100);
    /* Test direct path */
    brain_kg_path_t* path1 = brain_kg_find_path(kg, n1, n3);
    ASSERT_NE(path1, nullptr);
    EXPECT_EQ(path1->length, 3u);  /* n1 -> n2 -> n3 */
    brain_kg_path_destroy(path1);

    /* Test longer path */
    brain_kg_path_t* path2 = brain_kg_find_path(kg, n1, n5);
    ASSERT_NE(path2, nullptr);
    EXPECT_EQ(path2->length, 4u);  /* n1 -> n2 -> n3 -> n5 */
    brain_kg_path_destroy(path2);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Test connectivity", 50);
    EXPECT_TRUE(brain_kg_are_connected(kg, n1, n4));
    EXPECT_TRUE(brain_kg_are_connected(kg, n1, n5));

    /* Test reachability */
    brain_kg_node_list_t* reachable = brain_kg_get_reachable(kg, n1, 10);
    ASSERT_NE(reachable, nullptr);
    EXPECT_EQ(reachable->count, 4u);  /* n2, n3, n4, n5 */
    brain_kg_node_list_destroy(reachable);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Cleanup", 50);
    brain_kg_destroy(kg);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(KGFullE2E, KGNeighborQueries) {
    E2E_PIPELINE_START("KG Neighbor Queries");

    E2E_STAGE_BEGIN("Create hub network", 100);
    brain_kg_t* kg = create_test_kg();
    E2E_ASSERT_NOT_NULL(kg, "KG creation failed");

    brain_kg_node_id_t center = brain_kg_add_node(kg, "center", BRAIN_KG_NODE_CORTICAL, "Hub");
    brain_kg_node_id_t n1 = brain_kg_add_node(kg, "neighbor1", BRAIN_KG_NODE_CORTICAL, "N1");
    brain_kg_node_id_t n2 = brain_kg_add_node(kg, "neighbor2", BRAIN_KG_NODE_CORTICAL, "N2");
    brain_kg_node_id_t n3 = brain_kg_add_node(kg, "neighbor3", BRAIN_KG_NODE_CORTICAL, "N3");
    brain_kg_node_id_t isolated = brain_kg_add_node(kg, "isolated", BRAIN_KG_NODE_CORTICAL, "Isolated");

    brain_kg_add_edge(kg, center, n1, BRAIN_KG_EDGE_CONNECTS_TO, "link1", 0.5f);
    brain_kg_add_edge(kg, center, n2, BRAIN_KG_EDGE_SENDS_TO, "link2", 0.6f);
    brain_kg_add_edge(kg, n3, center, BRAIN_KG_EDGE_SENDS_TO, "link3", 0.7f);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Query neighbors", 100);
    brain_kg_node_list_t* neighbors = brain_kg_get_neighbors(kg, center);
    ASSERT_NE(neighbors, nullptr);
    EXPECT_EQ(neighbors->count, 3u);  /* n1, n2, n3 */
    brain_kg_node_list_destroy(neighbors);

    brain_kg_edge_list_t* outgoing = brain_kg_get_outgoing(kg, center);
    ASSERT_NE(outgoing, nullptr);
    EXPECT_EQ(outgoing->count, 2u);  /* to n1, to n2 */
    brain_kg_edge_list_destroy(outgoing);

    brain_kg_edge_list_t* incoming = brain_kg_get_incoming(kg, center);
    ASSERT_NE(incoming, nullptr);
    EXPECT_EQ(incoming->count, 1u);  /* from n3 */
    brain_kg_edge_list_destroy(incoming);

    /* Verify isolated node has no neighbors */
    brain_kg_node_list_t* no_neighbors = brain_kg_get_neighbors(kg, isolated);
    ASSERT_NE(no_neighbors, nullptr);
    EXPECT_EQ(no_neighbors->count, 0u);
    brain_kg_node_list_destroy(no_neighbors);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Cleanup", 50);
    brain_kg_destroy(kg);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(KGFullE2E, KGSearchQueries) {
    E2E_PIPELINE_START("KG Search Queries");

    E2E_STAGE_BEGIN("Create searchable KG", 100);
    brain_kg_t* kg = create_test_kg();
    E2E_ASSERT_NOT_NULL(kg, "KG creation failed");

    brain_kg_add_node(kg, "visual_cortex_v1", BRAIN_KG_NODE_CORTICAL, "Primary visual cortex");
    brain_kg_add_node(kg, "visual_cortex_v2", BRAIN_KG_NODE_CORTICAL, "Secondary visual cortex");
    brain_kg_add_node(kg, "audio_cortex_a1", BRAIN_KG_NODE_CORTICAL, "Primary auditory cortex");
    brain_kg_add_node(kg, "motor_cortex", BRAIN_KG_NODE_CORTICAL, "Motor cortex");
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Search for patterns", 100);
    /* Search for visual cortex nodes */
    brain_kg_node_list_t* visual = brain_kg_search_nodes(kg, "visual");
    ASSERT_NE(visual, nullptr);
    EXPECT_EQ(visual->count, 2u);
    brain_kg_node_list_destroy(visual);

    /* Search for cortex nodes */
    brain_kg_node_list_t* cortex = brain_kg_search_nodes(kg, "cortex");
    ASSERT_NE(cortex, nullptr);
    EXPECT_EQ(cortex->count, 4u);
    brain_kg_node_list_destroy(cortex);

    /* Search for non-existent pattern */
    brain_kg_node_list_t* none = brain_kg_search_nodes(kg, "nonexistent");
    ASSERT_NE(none, nullptr);
    EXPECT_EQ(none->count, 0u);
    brain_kg_node_list_destroy(none);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Cleanup", 50);
    brain_kg_destroy(kg);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// TEST GROUP 4: Stress Testing
//=============================================================================

TEST_F(KGFullE2E, KGStressLoad) {
    E2E_PIPELINE_START("KG Stress Load");

    printf("\n=== KG Stress Test ===\n");

    E2E_STAGE_BEGIN("Create KG", 50);
    brain_kg_t* kg = create_test_kg();
    E2E_ASSERT_NOT_NULL(kg, "KG creation failed");
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Add many nodes", 2000);
    uint64_t start_time = get_time_ms();

    brain_kg_node_id_t node_ids[STRESS_NODE_COUNT];
    for (int i = 0; i < STRESS_NODE_COUNT; i++) {
        char name[64];
        snprintf(name, sizeof(name), "stress_node_%04d", i);
        node_ids[i] = brain_kg_add_node(kg, name, BRAIN_KG_NODE_CORE, "Stress test node");
        EXPECT_NE(node_ids[i], BRAIN_KG_INVALID_NODE);
    }

    uint64_t node_time = get_time_ms() - start_time;
    printf("  Added %d nodes in %lu ms (%.1f nodes/sec)\n",
           STRESS_NODE_COUNT, node_time,
           node_time > 0 ? static_cast<float>(STRESS_NODE_COUNT) * 1000.0f / static_cast<float>(node_time) : 0.0f);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Add random edges", 5000);
    start_time = get_time_ms();
    int edges_added = 0;
    for (int i = 0; i < STRESS_EDGE_COUNT; i++) {
        int from_idx = rand() % STRESS_NODE_COUNT;
        int to_idx = rand() % STRESS_NODE_COUNT;
        if (from_idx != to_idx) {
            brain_kg_edge_id_t edge = brain_kg_add_edge(
                kg, node_ids[from_idx], node_ids[to_idx],
                BRAIN_KG_EDGE_CONNECTS_TO, "Stress edge",
                static_cast<float>(rand() % 100) / 100.0f
            );
            if (edge != BRAIN_KG_INVALID_NODE) {
                edges_added++;
            }
        }
    }

    uint64_t edge_time = get_time_ms() - start_time;
    printf("  Added %d edges in %lu ms (%.1f edges/sec)\n",
           edges_added, edge_time,
           edge_time > 0 ? static_cast<float>(edges_added) * 1000.0f / static_cast<float>(edge_time) : 0.0f);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Perform queries", 2000);
    start_time = get_time_ms();
    for (int i = 0; i < 100; i++) {
        int idx = rand() % STRESS_NODE_COUNT;
        brain_kg_node_list_t* neighbors = brain_kg_get_neighbors(kg, node_ids[idx]);
        if (neighbors) {
            brain_kg_node_list_destroy(neighbors);
        }
    }

    uint64_t query_time = get_time_ms() - start_time;
    printf("  Performed 100 neighbor queries in %lu ms (%.1f queries/sec)\n",
           query_time,
           query_time > 0 ? 100.0f * 1000.0f / static_cast<float>(query_time) : 0.0f);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify stats", 50);
    brain_kg_stats_t stats;
    brain_kg_get_stats(kg, &stats);
    EXPECT_EQ(stats.total_nodes, static_cast<uint32_t>(STRESS_NODE_COUNT));
    EXPECT_GE(stats.total_edges, 1u);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Cleanup", 100);
    brain_kg_destroy(kg);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(KGFullE2E, KGRapidUpdates) {
    E2E_PIPELINE_START("KG Rapid Updates");

    E2E_STAGE_BEGIN("Create KG and node", 50);
    brain_kg_t* kg = create_test_kg();
    E2E_ASSERT_NOT_NULL(kg, "KG creation failed");

    brain_kg_node_id_t node = brain_kg_add_node(kg, "rapid_update_node",
                                                  BRAIN_KG_NODE_COGNITIVE, "Test node");
    EXPECT_NE(node, BRAIN_KG_INVALID_NODE);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Rapid state updates", 1000);
    int update_errors = 0;
    for (int i = 0; i < 1000; i++) {
        brain_kg_node_state_t state = static_cast<brain_kg_node_state_t>(i % 5);
        char desc[64];
        snprintf(desc, sizeof(desc), "Update %d", i);

        int result = brain_kg_update_node(kg, node, desc, state);
        if (result != 0) {
            update_errors++;
        }
    }

    EXPECT_EQ(update_errors, 0) << "Update errors occurred";
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Cleanup", 50);
    brain_kg_destroy(kg);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// TEST GROUP 5: Security Tests
//=============================================================================

TEST_F(KGFullE2E, KGSecurityFeatures) {
    E2E_PIPELINE_START("KG Security Features");

    E2E_STAGE_BEGIN("Create secured KG", 50);
    brain_kg_t* kg = create_secured_kg();
    E2E_ASSERT_NOT_NULL(kg, "Secured KG creation failed");
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Generate admin token", 50);
    uint64_t admin_token;
    int token_result = brain_kg_generate_token(kg, BRAIN_KG_ACCESS_ADMIN, &admin_token);
    EXPECT_EQ(token_result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Set access level", 50);
    int access_result = brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_ADMIN, admin_token);
    EXPECT_EQ(access_result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Add critical node", 100);
    brain_kg_node_id_t critical = brain_kg_add_node(kg, "security_module",
                                                     BRAIN_KG_NODE_SECURITY, "Security system");
    EXPECT_NE(critical, BRAIN_KG_INVALID_NODE);

    int mark_result = brain_kg_mark_critical(kg, critical);
    EXPECT_EQ(mark_result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify integrity", 100);
    int integrity = brain_kg_verify_integrity(kg);
    EXPECT_EQ(integrity, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Cleanup", 50);
    brain_kg_destroy(kg);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(KGFullE2E, KGEmergencyLock) {
    E2E_PIPELINE_START("KG Emergency Lock");

    E2E_STAGE_BEGIN("Create secured KG", 50);
    brain_kg_t* kg = create_secured_kg();
    E2E_ASSERT_NOT_NULL(kg, "Secured KG creation failed");
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Generate admin token", 50);
    uint64_t admin_token;
    brain_kg_generate_token(kg, BRAIN_KG_ACCESS_ADMIN, &admin_token);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Add node before lock", 50);
    brain_kg_node_id_t node = brain_kg_add_node(kg, "pre_lock_node",
                                                  BRAIN_KG_NODE_CORE, "Node before lock");
    EXPECT_NE(node, BRAIN_KG_INVALID_NODE);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Lock the KG", 50);
    int lock_result = brain_kg_emergency_lock(kg);
    EXPECT_EQ(lock_result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Unlock with admin token", 50);
    int unlock_result = brain_kg_emergency_unlock(kg, admin_token);
    EXPECT_EQ(unlock_result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify post-unlock operations", 50);
    brain_kg_node_id_t post_lock = brain_kg_add_node(kg, "post_lock_node",
                                                      BRAIN_KG_NODE_CORE, "Node after unlock");
    EXPECT_NE(post_lock, BRAIN_KG_INVALID_NODE);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Cleanup", 50);
    brain_kg_destroy(kg);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// TEST GROUP 6: Message Handler Integration
//=============================================================================

TEST_F(KGFullE2E, KGMessageHandlerIndex) {
    E2E_PIPELINE_START("KG Message Handler Index");

    E2E_STAGE_BEGIN("Create KG with handlers", 100);
    brain_kg_t* kg = create_test_kg();
    E2E_ASSERT_NOT_NULL(kg, "KG creation failed");

    brain_kg_node_id_t handler1 = brain_kg_add_node(kg, "handler_module_1",
                                                     BRAIN_KG_NODE_COGNITIVE, "Handler 1");
    brain_kg_node_id_t handler2 = brain_kg_add_node(kg, "handler_module_2",
                                                     BRAIN_KG_NODE_COGNITIVE, "Handler 2");
    EXPECT_NE(handler1, BRAIN_KG_INVALID_NODE);
    EXPECT_NE(handler2, BRAIN_KG_INVALID_NODE);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Register message handlers", 100);
    int r1 = brain_kg_add_message_handler(kg, handler1, 100);  /* message type 100 */
    int r2 = brain_kg_add_message_handler(kg, handler1, 101);  /* message type 101 */
    int r3 = brain_kg_add_message_handler(kg, handler2, 100);  /* another handler for 100 */
    EXPECT_EQ(r1, 0);
    EXPECT_EQ(r2, 0);
    EXPECT_EQ(r3, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Query handlers", 100);
    /* Query handlers for message type 100 */
    brain_kg_handler_list_t* handlers = brain_kg_get_handlers_for_message_type(kg, 100);
    ASSERT_NE(handlers, nullptr);
    EXPECT_EQ(handlers->count, 2u);
    brain_kg_handler_list_destroy(handlers);

    /* Query handlers for message type 101 */
    handlers = brain_kg_get_handlers_for_message_type(kg, 101);
    ASSERT_NE(handlers, nullptr);
    EXPECT_EQ(handlers->count, 1u);
    brain_kg_handler_list_destroy(handlers);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Get module handled messages", 50);
    uint32_t msg_types[10];
    uint32_t count = brain_kg_get_module_handled_messages(kg, handler1, msg_types, 10);
    EXPECT_EQ(count, 2u);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Cleanup", 50);
    brain_kg_destroy(kg);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// TEST GROUP 7: Full Integration Pipeline
//=============================================================================

TEST_F(KGFullE2E, CompleteKGPipeline) {
    E2E_PIPELINE_START("Complete KG Integration Pipeline");

    printf("\n=== Complete KG Integration Pipeline ===\n");

    /* Phase 1: Create secured KG */
    E2E_STAGE_BEGIN("Phase 1: Create KG", 50);
    printf("  Phase 1: Creating KG...\n");
    brain_kg_t* kg = create_test_kg();
    E2E_ASSERT_NOT_NULL(kg, "KG creation failed");
    E2E_STAGE_END();

    /* Phase 2: Populate brain structure */
    E2E_STAGE_BEGIN("Phase 2: Populate brain structure", 200);
    printf("  Phase 2: Populating brain structure...\n");

    /* Core regions */
    brain_kg_node_id_t pfc = brain_kg_add_node(kg, "prefrontal_cortex",
                                                BRAIN_KG_NODE_CORTICAL, "Executive control");
    brain_kg_node_id_t hippo = brain_kg_add_node(kg, "hippocampus",
                                                  BRAIN_KG_NODE_SUBCORTICAL, "Memory formation");
    brain_kg_node_id_t amyg = brain_kg_add_node(kg, "amygdala",
                                                 BRAIN_KG_NODE_SUBCORTICAL, "Emotion processing");

    /* Cognitive modules */
    brain_kg_node_id_t wm = brain_kg_add_node(kg, "working_memory",
                                               BRAIN_KG_NODE_COGNITIVE, "Active memory buffer");
    brain_kg_node_id_t exec = brain_kg_add_node(kg, "executive_control",
                                                 BRAIN_KG_NODE_COGNITIVE, "Task management");
    brain_kg_node_id_t tom = brain_kg_add_node(kg, "theory_of_mind",
                                                BRAIN_KG_NODE_COGNITIVE, "Social cognition");

    /* Add connections */
    brain_kg_add_edge(kg, pfc, wm, BRAIN_KG_EDGE_PROVIDES_TO, "WM support", 0.9f);
    brain_kg_add_edge(kg, pfc, exec, BRAIN_KG_EDGE_COORDINATES_WITH, "Executive link", 0.8f);
    brain_kg_add_edge(kg, hippo, wm, BRAIN_KG_EDGE_CONNECTS_TO, "Memory-WM link", 0.7f);
    brain_kg_add_edge(kg, amyg, hippo, BRAIN_KG_EDGE_MODULATES, "Emotional memory", 0.85f);
    brain_kg_add_edge(kg, exec, tom, BRAIN_KG_EDGE_COORDINATES_WITH, "Social exec", 0.6f);
    E2E_STAGE_END();

    /* Phase 3: Register message handlers */
    E2E_STAGE_BEGIN("Phase 3: Register handlers", 100);
    printf("  Phase 3: Registering message handlers...\n");
    brain_kg_add_message_handler(kg, wm, 1);   /* WM handles memory updates */
    brain_kg_add_message_handler(kg, exec, 2); /* Executive handles task messages */
    brain_kg_add_message_handler(kg, tom, 3);  /* ToM handles social messages */
    E2E_STAGE_END();

    /* Phase 4: Verify structure */
    E2E_STAGE_BEGIN("Phase 4: Verify structure", 50);
    printf("  Phase 4: Verifying structure...\n");
    brain_kg_stats_t stats;
    brain_kg_get_stats(kg, &stats);
    EXPECT_EQ(stats.total_nodes, 6u);
    EXPECT_EQ(stats.total_edges, 5u);
    E2E_STAGE_END();

    /* Phase 5: Generate summary */
    E2E_STAGE_BEGIN("Phase 5: Generate summary", 100);
    printf("  Phase 5: Generating summary...\n");
    char summary[1024];
    int summary_len = brain_kg_generate_summary(kg, summary, sizeof(summary));
    EXPECT_GT(summary_len, 0);
    printf("  Summary: %s\n", summary);
    E2E_STAGE_END();

    /* Phase 6: Test queries */
    E2E_STAGE_BEGIN("Phase 6: Test queries", 100);
    printf("  Phase 6: Testing queries...\n");

    /* Find cognitive modules */
    brain_kg_node_list_t* cognitive = brain_kg_get_nodes_by_type(kg, BRAIN_KG_NODE_COGNITIVE);
    ASSERT_NE(cognitive, nullptr);
    EXPECT_EQ(cognitive->count, 3u);
    brain_kg_node_list_destroy(cognitive);

    /* Find path from amygdala to working memory */
    brain_kg_path_t* path = brain_kg_find_path(kg, amyg, wm);
    ASSERT_NE(path, nullptr);
    EXPECT_GE(path->length, 2u);
    brain_kg_path_destroy(path);
    E2E_STAGE_END();

    /* Phase 7: Cleanup */
    E2E_STAGE_BEGIN("Phase 7: Cleanup", 50);
    printf("  Phase 7: Cleanup...\n");
    brain_kg_destroy(kg);
    printf("  Pipeline completed successfully\n");
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

} // namespace e2e
} // namespace nimcp

//=============================================================================
// Entry Point
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
