/**
 * @file test_motor_kg_wiring_integration.cpp
 * @brief Integration tests for Motor Cortex Knowledge Graph wiring
 *
 * WHAT: Tests Motor Cortex integration with brain's internal Knowledge Graph
 * WHY:  Ensure proper semantic representation and message routing via KG
 * HOW:  Test node registration, edge creation, message handler mapping, and queries
 *
 * INTEGRATION POINTS:
 * - Brain KG node registration
 * - Motor component hierarchy (M1, premotor, SMA)
 * - Message-type handler mapping
 * - Path finding through motor pathways
 * - State synchronization with KG
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/brain/regions/motor/nimcp_motor_adapter.h"
#include "core/brain/nimcp_brain_kg.h"
#include "async/nimcp_bio_async.h"
#include "utils/logging/nimcp_logging.h"

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class MotorKGWiringTest : public ::testing::Test {
protected:
    motor_adapter_t* adapter;
    motor_config_t config;
    brain_kg_t* kg;

    void SetUp() override {
        /* Create brain knowledge graph */
        brain_kg_config_t kg_config;
        brain_kg_default_config(&kg_config);
        kg_config.enable_security = false;
        kg_config.enable_statistics = true;
        kg = brain_kg_create(&kg_config);
        ASSERT_NE(nullptr, kg) << "Failed to create brain KG";

        /* Configure motor adapter */
        config = motor_default_config();
        config.enable_bio_async = false;
        config.enable_training = true;
        config.enable_events = true;
        config.enable_premotor = true;
        config.enable_sma = true;

        adapter = motor_create(&config);
        ASSERT_NE(nullptr, adapter) << "Failed to create Motor adapter";
    }

    void TearDown() override {
        if (adapter) {
            motor_destroy(adapter);
            adapter = nullptr;
        }
        if (kg) {
            brain_kg_destroy(kg);
            kg = nullptr;
        }
    }

    /* Helper to register motor nodes in KG */
    brain_kg_node_id_t RegisterMotorNode(const char* name, const char* desc) {
        return brain_kg_add_node(kg, name, BRAIN_KG_NODE_CORTICAL, desc);
    }
};

/*=============================================================================
 * KG NODE REGISTRATION TESTS
 *===========================================================================*/

TEST_F(MotorKGWiringTest, RegisterMotorCortexRootNode) {
    brain_kg_node_id_t root_id = RegisterMotorNode(
        "motor_cortex",
        "Primary motor cortex for movement planning and execution"
    );
    EXPECT_NE(BRAIN_KG_INVALID_NODE, root_id);

    /* Verify node was added */
    const brain_kg_node_t* node = brain_kg_get_node(kg, root_id);
    ASSERT_NE(nullptr, node);
    EXPECT_STREQ("motor_cortex", node->name);
    EXPECT_EQ(BRAIN_KG_NODE_CORTICAL, node->type);
}

TEST_F(MotorKGWiringTest, RegisterMotorComponentHierarchy) {
    /* Register motor cortex root */
    brain_kg_node_id_t root_id = RegisterMotorNode(
        "motor_cortex", "Motor cortex root"
    );
    ASSERT_NE(BRAIN_KG_INVALID_NODE, root_id);

    /* Register M1 (primary motor cortex) */
    brain_kg_node_id_t m1_id = RegisterMotorNode(
        "m1_primary_motor", "Primary motor cortex - final motor output"
    );
    ASSERT_NE(BRAIN_KG_INVALID_NODE, m1_id);

    /* Register premotor cortex */
    brain_kg_node_id_t premotor_id = RegisterMotorNode(
        "premotor_cortex", "Premotor area - movement preparation"
    );
    ASSERT_NE(BRAIN_KG_INVALID_NODE, premotor_id);

    /* Register SMA (supplementary motor area) */
    brain_kg_node_id_t sma_id = RegisterMotorNode(
        "sma_supplementary", "Supplementary motor area - sequence planning"
    );
    ASSERT_NE(BRAIN_KG_INVALID_NODE, sma_id);

    /* Verify all nodes exist */
    brain_kg_stats_t stats;
    EXPECT_EQ(0, brain_kg_get_stats(kg, &stats));
    EXPECT_GE(stats.total_nodes, 4u);
}

TEST_F(MotorKGWiringTest, RegisterMotorEffectorNodes) {
    /* Register nodes for each motor region/effector */
    const char* effector_names[] = {
        "motor_hand_right", "motor_hand_left",
        "motor_arm_right", "motor_arm_left",
        "motor_leg_right", "motor_leg_left",
        "motor_foot_right", "motor_foot_left",
        "motor_face", "motor_trunk", "motor_eye"
    };

    brain_kg_node_id_t effector_ids[MOTOR_REGION_COUNT];
    for (int i = 0; i < MOTOR_REGION_COUNT; i++) {
        effector_ids[i] = RegisterMotorNode(effector_names[i],
            "Motor effector region");
        EXPECT_NE(BRAIN_KG_INVALID_NODE, effector_ids[i])
            << "Failed to register effector: " << effector_names[i];
    }
}

TEST_F(MotorKGWiringTest, RegisterMotorMetadata) {
    brain_kg_node_id_t motor_id = RegisterMotorNode(
        "motor_cortex", "Motor cortex"
    );
    ASSERT_NE(BRAIN_KG_INVALID_NODE, motor_id);

    /* Add metadata about motor capabilities */
    EXPECT_EQ(0, brain_kg_add_metadata(kg, motor_id,
        "capabilities", "movement_planning,trajectory_generation,motor_learning"));
    EXPECT_EQ(0, brain_kg_add_metadata(kg, motor_id,
        "somatotopic_regions", "11"));
    EXPECT_EQ(0, brain_kg_add_metadata(kg, motor_id,
        "movement_types", "discrete,serial,continuous,ballistic,corrective"));

    /* Verify metadata was stored */
    const brain_kg_node_t* node = brain_kg_get_node(kg, motor_id);
    ASSERT_NE(nullptr, node);
    EXPECT_GE(node->metadata_count, 3u);
}

/*=============================================================================
 * KG EDGE CREATION TESTS
 *===========================================================================*/

TEST_F(MotorKGWiringTest, CreateMotorHierarchyEdges) {
    /* Create nodes */
    brain_kg_node_id_t root_id = RegisterMotorNode("motor_cortex", "Root");
    brain_kg_node_id_t m1_id = RegisterMotorNode("m1_primary", "M1");
    brain_kg_node_id_t premotor_id = RegisterMotorNode("premotor", "Premotor");
    brain_kg_node_id_t sma_id = RegisterMotorNode("sma", "SMA");

    /* Create hierarchy edges (parent -> child) */
    brain_kg_edge_id_t e1 = brain_kg_add_edge(kg, root_id, m1_id,
        BRAIN_KG_EDGE_CONNECTS_TO, "Motor hierarchy", 1.0f);
    brain_kg_edge_id_t e2 = brain_kg_add_edge(kg, root_id, premotor_id,
        BRAIN_KG_EDGE_CONNECTS_TO, "Motor hierarchy", 1.0f);
    brain_kg_edge_id_t e3 = brain_kg_add_edge(kg, root_id, sma_id,
        BRAIN_KG_EDGE_CONNECTS_TO, "Motor hierarchy", 1.0f);

    EXPECT_NE(BRAIN_KG_INVALID_NODE, e1);
    EXPECT_NE(BRAIN_KG_INVALID_NODE, e2);
    EXPECT_NE(BRAIN_KG_INVALID_NODE, e3);

    /* Verify edges exist */
    brain_kg_edge_list_t* outgoing = brain_kg_get_outgoing(kg, root_id);
    ASSERT_NE(nullptr, outgoing);
    EXPECT_EQ(3u, outgoing->count);
    brain_kg_edge_list_destroy(outgoing);
}

TEST_F(MotorKGWiringTest, CreateMotorProcessingEdges) {
    /* Create nodes for motor processing pipeline */
    brain_kg_node_id_t sma_id = RegisterMotorNode("sma", "Sequence planning");
    brain_kg_node_id_t premotor_id = RegisterMotorNode("premotor", "Movement prep");
    brain_kg_node_id_t m1_id = RegisterMotorNode("m1", "Motor output");
    brain_kg_node_id_t cerebellum_id = brain_kg_add_node(kg, "cerebellum",
        BRAIN_KG_NODE_SUBCORTICAL, "Motor coordination");
    brain_kg_node_id_t basal_ganglia_id = brain_kg_add_node(kg, "basal_ganglia",
        BRAIN_KG_NODE_SUBCORTICAL, "Action selection");

    /* Create processing edges */
    /* SMA -> Premotor -> M1 (planning pipeline) */
    brain_kg_add_edge(kg, sma_id, premotor_id,
        BRAIN_KG_EDGE_SENDS_TO, "Sequence to preparation", 0.9f);
    brain_kg_add_edge(kg, premotor_id, m1_id,
        BRAIN_KG_EDGE_SENDS_TO, "Preparation to execution", 0.9f);

    /* Cerebellum feedback */
    brain_kg_add_edge(kg, cerebellum_id, m1_id,
        BRAIN_KG_EDGE_MODULATES, "Timing and coordination", 0.8f);

    /* Basal ganglia action selection */
    brain_kg_add_edge(kg, basal_ganglia_id, premotor_id,
        BRAIN_KG_EDGE_MODULATES, "Action selection signal", 0.7f);

    /* Verify path from SMA to M1 */
    brain_kg_path_t* path = brain_kg_find_path(kg, sma_id, m1_id);
    ASSERT_NE(nullptr, path);
    EXPECT_GE(path->length, 2u);
    brain_kg_path_destroy(path);
}

TEST_F(MotorKGWiringTest, CreateEffectorOutputEdges) {
    brain_kg_node_id_t m1_id = RegisterMotorNode("m1", "Primary motor");
    brain_kg_node_id_t hand_r_id = RegisterMotorNode("hand_right", "Right hand");
    brain_kg_node_id_t hand_l_id = RegisterMotorNode("hand_left", "Left hand");

    /* M1 sends motor commands to effectors */
    brain_kg_edge_id_t e1 = brain_kg_add_edge(kg, m1_id, hand_r_id,
        BRAIN_KG_EDGE_SENDS_TO, "Motor command", 1.0f);
    brain_kg_edge_id_t e2 = brain_kg_add_edge(kg, m1_id, hand_l_id,
        BRAIN_KG_EDGE_SENDS_TO, "Motor command", 1.0f);

    EXPECT_NE(BRAIN_KG_INVALID_NODE, e1);
    EXPECT_NE(BRAIN_KG_INVALID_NODE, e2);

    /* Verify M1 is connected to effectors */
    EXPECT_TRUE(brain_kg_are_connected(kg, m1_id, hand_r_id));
    EXPECT_TRUE(brain_kg_are_connected(kg, m1_id, hand_l_id));
}

/*=============================================================================
 * MESSAGE HANDLER MAPPING TESTS
 *===========================================================================*/

TEST_F(MotorKGWiringTest, RegisterMotorMessageHandlers) {
    brain_kg_node_id_t motor_id = RegisterMotorNode("motor_cortex", "Motor");

    /* Register motor-related message handlers */
    /* These message type values are conceptual - actual values from bio_async */
    uint32_t msg_movement_request = 0x100;
    uint32_t msg_feedback_signal = 0x101;
    uint32_t msg_learning_update = 0x102;

    EXPECT_EQ(0, brain_kg_add_message_handler(kg, motor_id, msg_movement_request));
    EXPECT_EQ(0, brain_kg_add_message_handler(kg, motor_id, msg_feedback_signal));
    EXPECT_EQ(0, brain_kg_add_message_handler(kg, motor_id, msg_learning_update));

    /* Verify handlers can be looked up */
    brain_kg_handler_list_t* handlers = brain_kg_get_handlers_for_message_type(
        kg, msg_movement_request);
    ASSERT_NE(nullptr, handlers);
    EXPECT_GE(handlers->count, 1u);

    /* Motor should be in the handler list */
    bool found_motor = false;
    for (uint32_t i = 0; i < handlers->count; i++) {
        if (handlers->handlers[i] == motor_id) {
            found_motor = true;
            break;
        }
    }
    EXPECT_TRUE(found_motor);
    brain_kg_handler_list_destroy(handlers);
}

TEST_F(MotorKGWiringTest, QueryModuleHandledMessages) {
    brain_kg_node_id_t motor_id = RegisterMotorNode("motor_cortex", "Motor");

    /* Register multiple message handlers */
    uint32_t msg_types[] = {0x100, 0x101, 0x102, 0x103};
    for (int i = 0; i < 4; i++) {
        brain_kg_add_message_handler(kg, motor_id, msg_types[i]);
    }

    /* Query what messages the motor module handles */
    uint32_t handled[16];
    uint32_t count = brain_kg_get_module_handled_messages(
        kg, motor_id, handled, 16);
    EXPECT_EQ(4u, count);
}

TEST_F(MotorKGWiringTest, RemoveMessageHandler) {
    brain_kg_node_id_t motor_id = RegisterMotorNode("motor_cortex", "Motor");
    uint32_t msg_type = 0x200;

    /* Add then remove handler */
    EXPECT_EQ(0, brain_kg_add_message_handler(kg, motor_id, msg_type));

    brain_kg_handler_list_t* before = brain_kg_get_handlers_for_message_type(
        kg, msg_type);
    ASSERT_NE(nullptr, before);
    uint32_t count_before = before->count;
    brain_kg_handler_list_destroy(before);

    EXPECT_EQ(0, brain_kg_remove_message_handler(kg, motor_id, msg_type));

    brain_kg_handler_list_t* after = brain_kg_get_handlers_for_message_type(
        kg, msg_type);
    uint32_t count_after = after ? after->count : 0;
    if (after) brain_kg_handler_list_destroy(after);

    EXPECT_LT(count_after, count_before);
}

/*=============================================================================
 * GRAPH TRAVERSAL TESTS
 *===========================================================================*/

TEST_F(MotorKGWiringTest, FindPathThroughMotorHierarchy) {
    /* Build a motor processing hierarchy */
    brain_kg_node_id_t pfc_id = brain_kg_add_node(kg, "prefrontal",
        BRAIN_KG_NODE_CORTICAL, "Executive control");
    brain_kg_node_id_t sma_id = RegisterMotorNode("sma", "Sequence planning");
    brain_kg_node_id_t premotor_id = RegisterMotorNode("premotor", "Preparation");
    brain_kg_node_id_t m1_id = RegisterMotorNode("m1", "Primary motor");
    brain_kg_node_id_t spinal_id = brain_kg_add_node(kg, "spinal",
        BRAIN_KG_NODE_BRAINSTEM, "Spinal cord");

    /* Create pathway edges */
    brain_kg_add_edge(kg, pfc_id, sma_id, BRAIN_KG_EDGE_SENDS_TO, "Goal", 0.8f);
    brain_kg_add_edge(kg, sma_id, premotor_id, BRAIN_KG_EDGE_SENDS_TO, "Seq", 0.9f);
    brain_kg_add_edge(kg, premotor_id, m1_id, BRAIN_KG_EDGE_SENDS_TO, "Prep", 0.9f);
    brain_kg_add_edge(kg, m1_id, spinal_id, BRAIN_KG_EDGE_SENDS_TO, "Cmd", 1.0f);

    /* Find path from PFC to spinal */
    brain_kg_path_t* path = brain_kg_find_path(kg, pfc_id, spinal_id);
    ASSERT_NE(nullptr, path);
    EXPECT_EQ(5u, path->length); /* PFC -> SMA -> Premotor -> M1 -> Spinal */
    brain_kg_path_destroy(path);
}

TEST_F(MotorKGWiringTest, GetReachableFromMotor) {
    /* Create motor with connections */
    brain_kg_node_id_t motor_id = RegisterMotorNode("motor", "Motor");
    brain_kg_node_id_t cb_id = brain_kg_add_node(kg, "cerebellum",
        BRAIN_KG_NODE_SUBCORTICAL, "Cerebellum");
    brain_kg_node_id_t bg_id = brain_kg_add_node(kg, "basal_ganglia",
        BRAIN_KG_NODE_SUBCORTICAL, "Basal ganglia");
    brain_kg_node_id_t thal_id = brain_kg_add_node(kg, "thalamus",
        BRAIN_KG_NODE_SUBCORTICAL, "Thalamus");

    brain_kg_add_edge(kg, motor_id, cb_id, BRAIN_KG_EDGE_CONNECTS_TO, "", 0.8f);
    brain_kg_add_edge(kg, motor_id, bg_id, BRAIN_KG_EDGE_CONNECTS_TO, "", 0.7f);
    brain_kg_add_edge(kg, bg_id, thal_id, BRAIN_KG_EDGE_CONNECTS_TO, "", 0.6f);

    /* Get all nodes reachable from motor */
    brain_kg_node_list_t* reachable = brain_kg_get_reachable(kg, motor_id, 2);
    ASSERT_NE(nullptr, reachable);
    EXPECT_GE(reachable->count, 3u); /* CB, BG, Thalamus */
    brain_kg_node_list_destroy(reachable);
}

TEST_F(MotorKGWiringTest, GetMotorNeighbors) {
    brain_kg_node_id_t motor_id = RegisterMotorNode("motor", "Motor");
    brain_kg_node_id_t sensory_id = brain_kg_add_node(kg, "sensory",
        BRAIN_KG_NODE_PERCEPTION, "Sensory");
    brain_kg_node_id_t cognitive_id = brain_kg_add_node(kg, "cognitive",
        BRAIN_KG_NODE_COGNITIVE, "Cognitive");

    /* Bidirectional connections */
    brain_kg_add_edge(kg, sensory_id, motor_id,
        BRAIN_KG_EDGE_SENDS_TO, "Feedback", 0.8f);
    brain_kg_add_edge(kg, motor_id, cognitive_id,
        BRAIN_KG_EDGE_SENDS_TO, "Efference", 0.7f);

    brain_kg_node_list_t* neighbors = brain_kg_get_neighbors(kg, motor_id);
    ASSERT_NE(nullptr, neighbors);
    EXPECT_GE(neighbors->count, 2u);
    brain_kg_node_list_destroy(neighbors);
}

/*=============================================================================
 * STATE SYNCHRONIZATION TESTS
 *===========================================================================*/

TEST_F(MotorKGWiringTest, UpdateMotorNodeState) {
    brain_kg_node_id_t motor_id = RegisterMotorNode("motor", "Motor");

    /* Update state as motor progresses through lifecycle */
    EXPECT_EQ(0, brain_kg_update_node(kg, motor_id, NULL,
        BRAIN_KG_STATE_INITIALIZING));

    const brain_kg_node_t* node = brain_kg_get_node(kg, motor_id);
    EXPECT_EQ(BRAIN_KG_STATE_INITIALIZING, node->state);

    EXPECT_EQ(0, brain_kg_update_node(kg, motor_id, NULL,
        BRAIN_KG_STATE_ACTIVE));

    node = brain_kg_get_node(kg, motor_id);
    EXPECT_EQ(BRAIN_KG_STATE_ACTIVE, node->state);
}

TEST_F(MotorKGWiringTest, SetMotorModulePointer) {
    brain_kg_node_id_t motor_id = RegisterMotorNode("motor", "Motor");

    /* Associate KG node with actual motor adapter instance */
    EXPECT_EQ(0, brain_kg_set_module_ptr(kg, motor_id, adapter));

    const brain_kg_node_t* node = brain_kg_get_node(kg, motor_id);
    EXPECT_EQ((void*)adapter, node->module_ptr);
}

/*=============================================================================
 * QUERY API TESTS
 *===========================================================================*/

TEST_F(MotorKGWiringTest, SearchMotorNodes) {
    RegisterMotorNode("motor_cortex_m1", "M1");
    RegisterMotorNode("motor_cortex_premotor", "Premotor");
    RegisterMotorNode("motor_cortex_sma", "SMA");
    brain_kg_add_node(kg, "visual_cortex", BRAIN_KG_NODE_PERCEPTION, "Visual");

    /* Search for motor-related nodes */
    brain_kg_node_list_t* motor_nodes = brain_kg_search_nodes(kg, "motor");
    ASSERT_NE(nullptr, motor_nodes);
    EXPECT_EQ(3u, motor_nodes->count);
    brain_kg_node_list_destroy(motor_nodes);
}

TEST_F(MotorKGWiringTest, GetNodesByType) {
    RegisterMotorNode("m1", "M1");
    RegisterMotorNode("premotor", "Premotor");
    brain_kg_add_node(kg, "cerebellum", BRAIN_KG_NODE_SUBCORTICAL, "CB");
    brain_kg_add_node(kg, "basal_ganglia", BRAIN_KG_NODE_SUBCORTICAL, "BG");

    brain_kg_node_list_t* cortical = brain_kg_get_nodes_by_type(kg,
        BRAIN_KG_NODE_CORTICAL);
    ASSERT_NE(nullptr, cortical);
    EXPECT_EQ(2u, cortical->count);
    brain_kg_node_list_destroy(cortical);

    brain_kg_node_list_t* subcortical = brain_kg_get_nodes_by_type(kg,
        BRAIN_KG_NODE_SUBCORTICAL);
    ASSERT_NE(nullptr, subcortical);
    EXPECT_EQ(2u, subcortical->count);
    brain_kg_node_list_destroy(subcortical);
}

TEST_F(MotorKGWiringTest, GetHubNodes) {
    /* Create hub structure where motor has most connections */
    brain_kg_node_id_t motor_id = RegisterMotorNode("motor", "Motor - hub");
    brain_kg_node_id_t n1 = brain_kg_add_node(kg, "n1", BRAIN_KG_NODE_CORTICAL, "");
    brain_kg_node_id_t n2 = brain_kg_add_node(kg, "n2", BRAIN_KG_NODE_CORTICAL, "");
    brain_kg_node_id_t n3 = brain_kg_add_node(kg, "n3", BRAIN_KG_NODE_CORTICAL, "");
    brain_kg_node_id_t n4 = brain_kg_add_node(kg, "n4", BRAIN_KG_NODE_CORTICAL, "");

    /* Motor connects to all others */
    brain_kg_add_edge(kg, motor_id, n1, BRAIN_KG_EDGE_CONNECTS_TO, "", 0.5f);
    brain_kg_add_edge(kg, motor_id, n2, BRAIN_KG_EDGE_CONNECTS_TO, "", 0.5f);
    brain_kg_add_edge(kg, motor_id, n3, BRAIN_KG_EDGE_CONNECTS_TO, "", 0.5f);
    brain_kg_add_edge(kg, motor_id, n4, BRAIN_KG_EDGE_CONNECTS_TO, "", 0.5f);

    brain_kg_node_list_t* hubs = brain_kg_get_hubs(kg, 3);
    ASSERT_NE(nullptr, hubs);
    EXPECT_GT(hubs->count, 0u);
    /* Motor should be the most connected */
    EXPECT_EQ(motor_id, hubs->nodes[0]->id);
    brain_kg_node_list_destroy(hubs);
}

/*=============================================================================
 * STATISTICS TESTS
 *===========================================================================*/

TEST_F(MotorKGWiringTest, KGStatisticsTracking) {
    /* Add nodes and edges */
    brain_kg_node_id_t n1 = RegisterMotorNode("motor1", "M1");
    brain_kg_node_id_t n2 = RegisterMotorNode("motor2", "M2");
    brain_kg_add_edge(kg, n1, n2, BRAIN_KG_EDGE_CONNECTS_TO, "", 0.5f);

    brain_kg_stats_t stats;
    EXPECT_EQ(0, brain_kg_get_stats(kg, &stats));
    EXPECT_EQ(2u, stats.total_nodes);
    EXPECT_EQ(1u, stats.total_edges);
    EXPECT_GT(stats.nodes_by_type[BRAIN_KG_NODE_CORTICAL], 0u);
}

TEST_F(MotorKGWiringTest, GenerateKGSummary) {
    RegisterMotorNode("motor_m1", "Primary motor");
    RegisterMotorNode("motor_premotor", "Premotor");
    RegisterMotorNode("motor_sma", "SMA");

    char buffer[2048];
    memset(buffer, 0, sizeof(buffer));
    int chars = brain_kg_generate_summary(kg, buffer, sizeof(buffer));
    /* Summary generation should work - content format is implementation-defined */
    EXPECT_GE(chars, 0);
    /* If chars > 0, buffer should have content */
    if (chars > 0) {
        EXPECT_GT(strlen(buffer), 0u);
    }
}

/*=============================================================================
 * EDGE TYPE TESTS
 *===========================================================================*/

TEST_F(MotorKGWiringTest, GetEdgesByType) {
    brain_kg_node_id_t motor_id = RegisterMotorNode("motor", "Motor");
    brain_kg_node_id_t sensory_id = brain_kg_add_node(kg, "sensory",
        BRAIN_KG_NODE_PERCEPTION, "Sensory");
    brain_kg_node_id_t basal_id = brain_kg_add_node(kg, "basal",
        BRAIN_KG_NODE_SUBCORTICAL, "BG");

    /* Different edge types */
    brain_kg_add_edge(kg, sensory_id, motor_id,
        BRAIN_KG_EDGE_SENDS_TO, "Afferent", 0.8f);
    brain_kg_add_edge(kg, motor_id, sensory_id,
        BRAIN_KG_EDGE_SENDS_TO, "Efferent", 0.7f);
    brain_kg_add_edge(kg, basal_id, motor_id,
        BRAIN_KG_EDGE_MODULATES, "Selection", 0.6f);

    brain_kg_edge_list_t* modulates = brain_kg_get_edges_by_type(kg,
        BRAIN_KG_EDGE_MODULATES);
    ASSERT_NE(nullptr, modulates);
    EXPECT_EQ(1u, modulates->count);
    brain_kg_edge_list_destroy(modulates);

    brain_kg_edge_list_t* sends = brain_kg_get_edges_by_type(kg,
        BRAIN_KG_EDGE_SENDS_TO);
    ASSERT_NE(nullptr, sends);
    EXPECT_EQ(2u, sends->count);
    brain_kg_edge_list_destroy(sends);
}

/*=============================================================================
 * NODE REMOVAL TESTS
 *===========================================================================*/

TEST_F(MotorKGWiringTest, RemoveMotorNode) {
    brain_kg_node_id_t motor_id = RegisterMotorNode("motor", "Motor");
    brain_kg_node_id_t connected_id = RegisterMotorNode("connected", "Connected");

    brain_kg_add_edge(kg, motor_id, connected_id,
        BRAIN_KG_EDGE_CONNECTS_TO, "", 0.5f);

    brain_kg_stats_t before;
    brain_kg_get_stats(kg, &before);

    /* Remove motor node - should also remove edge */
    EXPECT_EQ(0, brain_kg_remove_node(kg, motor_id));

    brain_kg_stats_t after;
    brain_kg_get_stats(kg, &after);
    EXPECT_EQ(before.total_nodes - 1, after.total_nodes);
    EXPECT_EQ(before.total_edges - 1, after.total_edges);

    /* Motor node should not be findable */
    EXPECT_EQ(BRAIN_KG_INVALID_NODE, brain_kg_find_node(kg, "motor"));
}

/*=============================================================================
 * STRING CONVERSION TESTS
 *===========================================================================*/

TEST_F(MotorKGWiringTest, NodeTypeToString) {
    EXPECT_NE(nullptr, brain_kg_node_type_to_string(BRAIN_KG_NODE_CORTICAL));
    EXPECT_NE(nullptr, brain_kg_node_type_to_string(BRAIN_KG_NODE_SUBCORTICAL));
    EXPECT_NE(nullptr, brain_kg_node_type_to_string(BRAIN_KG_NODE_BRAINSTEM));
}

TEST_F(MotorKGWiringTest, EdgeTypeToString) {
    EXPECT_NE(nullptr, brain_kg_edge_type_to_string(BRAIN_KG_EDGE_CONNECTS_TO));
    EXPECT_NE(nullptr, brain_kg_edge_type_to_string(BRAIN_KG_EDGE_SENDS_TO));
    EXPECT_NE(nullptr, brain_kg_edge_type_to_string(BRAIN_KG_EDGE_MODULATES));
}

TEST_F(MotorKGWiringTest, NodeStateToString) {
    EXPECT_NE(nullptr, brain_kg_node_state_to_string(BRAIN_KG_STATE_ACTIVE));
    EXPECT_NE(nullptr, brain_kg_node_state_to_string(BRAIN_KG_STATE_DISABLED));
    EXPECT_NE(nullptr, brain_kg_node_state_to_string(BRAIN_KG_STATE_ERROR));
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
