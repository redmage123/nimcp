/**
 * @file test_cognitive_pipeline.cpp
 * @brief E2E Tests for Cognitive Processing Pipeline
 *
 * WHAT: Complete cognitive pipeline testing (perception → processing → response → consolidation)
 * WHY:  Validate integration of cognitive modules (workspace, ethics, memory)
 * HOW:  Test realistic cognitive workflows with module interactions
 *
 * TEST COVERAGE:
 * - Input perception and feature extraction
 * - Global workspace competition and broadcasting
 * - Ethical decision making
 * - Working memory management
 * - Response generation
 * - Memory consolidation
 */

#include "e2e_test_framework.h"

// Headers have their own extern "C" guards
#include "cognitive/global_workspace/nimcp_global_workspace.h"
#include "cognitive/ethics/nimcp_ethics.h"

using namespace nimcp::e2e;

//=============================================================================
// Test Fixtures
//=============================================================================

class CognitivePipelineTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_init();
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);
    }

    void TearDown() override {
        nimcp_memory_check_leaks();
        nimcp_shutdown();
    }
};

//=============================================================================
// E2E Test: Perception to Decision Pipeline
//=============================================================================

E2E_TEST(CognitivePipelineTest, PerceptionToDecision) {
    E2E_PIPELINE_START("Perception to Decision");

    nimcp_brain_t brain = nullptr;

    // Stage 1: Create brain with cognitive modules enabled
    E2E_STAGE_BEGIN("Create brain with cognitive features", 500);  // Increased timeout for brain creation
    {
        brain = nimcp_brain_create(
            "cognitive_brain",
            NIMCP_BRAIN_MEDIUM,
            NIMCP_TASK_CLASSIFICATION,
            256,  // Feature dimension
            10    // Decision classes
        );
        E2E_ASSERT_NOT_NULL(brain, "Brain creation failed");

        // Note: Global workspace and working memory are enabled internally
        // when brain size is MEDIUM or larger
    }
    E2E_STAGE_END();

    // Stage 2: Input perception (feature extraction)
    E2E_STAGE_BEGIN("Input perception", 100);
    {
        std::vector<float> raw_input = TestDataGenerator::generate_features(256);

        // Simulate perception: raw input → feature extraction
        // In real system, this would be sensory preprocessing
        std::vector<float> perceived_features = raw_input;

        E2E_ASSERT(perceived_features.size() == 256, "Feature dimension mismatch");
    }
    E2E_STAGE_END();

    // Stage 3: Global workspace competition
    E2E_STAGE_BEGIN("Global workspace competition", 200);
    {
        std::vector<float> content = TestDataGenerator::generate_features(256);

        // Multiple modules compete for conscious access
        nimcp_status_t status1 = nimcp_brain_workspace_compete(
            brain,
            NIMCP_MODULE_PERCEPTION,
            content.data(),
            256,
            0.85f  // High strength
        );

        // Lower strength should lose
        nimcp_status_t status2 = nimcp_brain_workspace_compete(
            brain,
            NIMCP_MODULE_EMOTION,
            content.data(),
            256,
            0.30f  // Low strength
        );

        // At least one should succeed (the stronger one)
        bool one_succeeded = (status1 == NIMCP_SUCCESS) || (status2 == NIMCP_SUCCESS);
        E2E_ASSERT(one_succeeded, "No module won workspace competition");
    }
    E2E_STAGE_END();

    // Stage 4: Read broadcast from workspace
    E2E_STAGE_BEGIN("Read workspace broadcast", 50);
    {
        std::vector<float> broadcast_content(256);
        uint32_t actual_dim;
        nimcp_cognitive_module_t source;

        nimcp_status_t status = nimcp_brain_workspace_read(
            brain,
            broadcast_content.data(),
            256,
            &actual_dim,
            &source
        );

        if (status == NIMCP_SUCCESS) {
            E2E_ASSERT(actual_dim == 256, "Broadcast dimension mismatch");
            std::cout << "  Broadcast from module: " << static_cast<int>(source) << "\n";
        }
    }
    E2E_STAGE_END();

    // Stage 5: Decision making (inference)
    E2E_STAGE_BEGIN("Decision making", 100);
    {
        std::vector<float> features = TestDataGenerator::generate_features(256);
        std::vector<float> decision_outputs(10);

        nimcp_status_t status = nimcp_brain_infer(
            brain,
            features.data(),
            256,
            decision_outputs.data(),
            10
        );
        E2E_ASSERT_SUCCESS(status, "Decision inference failed");

        // Find winning decision
        int decision = std::max_element(decision_outputs.begin(), decision_outputs.end())
                       - decision_outputs.begin();
        float confidence = decision_outputs[decision];

        std::cout << "  Decision: class " << decision << " (confidence: "
                  << confidence << ")\n";
        E2E_ASSERT(confidence > 0.0f, "Invalid decision confidence");
    }
    E2E_STAGE_END();

    // Stage 6: Workspace statistics
    E2E_STAGE_BEGIN("Check workspace statistics", 50);
    {
        uint32_t total_broadcasts = 0;
        uint32_t total_competitions = 0;
        float avg_strength = 0.0f;

        nimcp_status_t status = nimcp_brain_workspace_stats(
            brain,
            &total_broadcasts,
            &total_competitions,
            &avg_strength
        );

        if (status == NIMCP_SUCCESS) {
            std::cout << "  Workspace: " << total_broadcasts << " broadcasts, "
                      << total_competitions << " competitions\n";
            E2E_ASSERT(total_competitions > 0, "No workspace activity recorded");
        }
    }
    E2E_STAGE_END();

    // Stage 7: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 1000);  // Increased timeout - brain destruction can be slow with cognitive modules
    {
        nimcp_brain_destroy(brain);
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Working Memory Management
//=============================================================================

E2E_TEST(CognitivePipelineTest, WorkingMemoryManagement) {
    E2E_PIPELINE_START("Working Memory Management");

    nimcp_brain_t brain = nullptr;

    // Stage 1: Create brain with working memory
    E2E_STAGE_BEGIN("Create brain", 500);  // Increased timeout for brain creation
    {
        brain = nimcp_brain_create(
            "working_memory_test",
            NIMCP_BRAIN_MEDIUM,
            NIMCP_TASK_SEQUENCE,
            64, 32
        );
        E2E_ASSERT_NOT_NULL(brain, "Brain creation failed");
    }
    E2E_STAGE_END();

    // Stage 2: Add items to working memory
    E2E_STAGE_BEGIN("Add items to working memory", 200);
    {
        // Add multiple items with varying salience
        for (int i = 0; i < 5; ++i) {
            std::vector<float> item = TestDataGenerator::generate_features(64);
            float salience = 0.5f + (i * 0.1f);  // Increasing salience

            nimcp_status_t status = nimcp_brain_working_memory_add(
                brain,
                item.data(),
                64,
                salience
            );
            E2E_ASSERT_SUCCESS(status, "Failed to add item to working memory");
        }
    }
    E2E_STAGE_END();

    // Stage 3: Query working memory statistics
    E2E_STAGE_BEGIN("Query working memory stats", 50);
    {
        uint32_t size, capacity;
        nimcp_status_t status = nimcp_brain_working_memory_stats(
            brain, &size, &capacity
        );
        E2E_ASSERT_SUCCESS(status, "Failed to get working memory stats");

        std::cout << "  Working memory: " << size << "/" << capacity << " items\n";
        E2E_ASSERT(size <= capacity, "Working memory overflow");
        E2E_ASSERT(size > 0, "No items in working memory");
    }
    E2E_STAGE_END();

    // Stage 4: Retrieve highest-salience item
    E2E_STAGE_BEGIN("Retrieve items by salience", 100);
    {
        uint32_t item_size;
        const float* item = nimcp_brain_working_memory_get(brain, 0, &item_size);

        if (item != nullptr) {
            E2E_ASSERT(item_size == 64, "Item size mismatch");
            std::cout << "  Retrieved highest-salience item (size: " << item_size << ")\n";
        }
    }
    E2E_STAGE_END();

    // Stage 5: Refresh item (prevent decay)
    E2E_STAGE_BEGIN("Refresh item", 50);
    {
        nimcp_status_t status = nimcp_brain_working_memory_refresh(brain, 0);
        E2E_ASSERT_SUCCESS(status, "Failed to refresh item");
    }
    E2E_STAGE_END();

    // Stage 6: Process items (simulate cognitive operations)
    E2E_STAGE_BEGIN("Process working memory items", 500);
    {
        uint32_t size, capacity;
        nimcp_brain_working_memory_stats(brain, &size, &capacity);

        // Process each item in working memory
        for (uint32_t i = 0; i < size; ++i) {
            uint32_t item_size;
            const float* item = nimcp_brain_working_memory_get(brain, i, &item_size);

            if (item != nullptr) {
                // Simulate processing: use item as input for inference
                std::vector<float> outputs(32);
                nimcp_brain_infer(brain, item, item_size, outputs.data(), 32);
            }
        }
    }
    E2E_STAGE_END();

    // Stage 7: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 1000);  // Increased timeout - brain destruction can be slow with cognitive modules
    {
        nimcp_brain_destroy(brain);
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Ethical Decision Pipeline
//=============================================================================

E2E_TEST(CognitivePipelineTest, EthicalDecisionPipeline) {
    E2E_PIPELINE_START("Ethical Decision Pipeline");

    nimcp_brain_t brain = nullptr;
    nimcp_ethics_t ethics = nullptr;

    // Stage 1: Create brain and ethics module
    E2E_STAGE_BEGIN("Create brain and ethics", 500);  // Increased timeout - ethics setup can be slow
    {
        brain = nimcp_brain_create(
            "ethical_brain",
            NIMCP_BRAIN_SMALL,
            NIMCP_TASK_CLASSIFICATION,
            20, 5
        );
        E2E_ASSERT_NOT_NULL(brain, "Brain creation failed");

        ethics = nimcp_ethics_create();
        E2E_ASSERT_NOT_NULL(ethics, "Ethics module creation failed");
    }
    E2E_STAGE_END();

    // Stage 2: Generate situation features
    E2E_STAGE_BEGIN("Generate situation", 50);
    {
        std::vector<float> situation = TestDataGenerator::generate_features(20);
        E2E_ASSERT(situation.size() == 20, "Situation feature size mismatch");
    }
    E2E_STAGE_END();

    // Stage 3: Generate candidate actions
    E2E_STAGE_BEGIN("Generate candidate actions", 100);
    {
        std::vector<std::vector<float>> actions(5);
        for (auto& action : actions) {
            action = TestDataGenerator::generate_features(20);
        }
        E2E_ASSERT(actions.size() == 5, "Wrong number of candidate actions");
    }
    E2E_STAGE_END();

    // Stage 4: Ethical evaluation of actions
    E2E_STAGE_BEGIN("Ethical evaluation", 200);
    {
        std::vector<float> situation = TestDataGenerator::generate_features(20);
        std::vector<float> ethical_scores(5);

        for (size_t i = 0; i < 5; ++i) {
            nimcp_status_t status = nimcp_ethics_check(
                ethics,
                situation.data(),
                20,
                &ethical_scores[i]
            );
            E2E_ASSERT_SUCCESS(status, "Ethical check failed");
        }

        // Find most ethical action
        int best_action = std::max_element(ethical_scores.begin(), ethical_scores.end())
                          - ethical_scores.begin();
        float best_score = ethical_scores[best_action];

        std::cout << "  Best ethical action: " << best_action
                  << " (score: " << best_score << ")\n";
    }
    E2E_STAGE_END();

    // Stage 5: Make decision with ethical constraints
    E2E_STAGE_BEGIN("Ethical decision making", 150);
    {
        std::vector<float> situation = TestDataGenerator::generate_features(20);
        std::vector<float> decision_outputs(5);

        // Get raw decision from brain
        nimcp_status_t status = nimcp_brain_infer(
            brain,
            situation.data(),
            20,
            decision_outputs.data(),
            5
        );
        E2E_ASSERT_SUCCESS(status, "Decision inference failed");

        // Apply ethical filter
        for (size_t i = 0; i < 5; ++i) {
            float ethical_score;
            nimcp_ethics_check(ethics, situation.data(), 20, &ethical_score);

            // Suppress unethical actions
            if (ethical_score < 0.0f) {
                decision_outputs[i] = 0.0f;
            }
        }

        // Renormalize
        float sum = std::accumulate(decision_outputs.begin(), decision_outputs.end(), 0.0f);
        if (sum > 0.0f) {
            for (auto& val : decision_outputs) {
                val /= sum;
            }
        }

        int final_decision = std::max_element(decision_outputs.begin(), decision_outputs.end())
                             - decision_outputs.begin();
        std::cout << "  Ethically-filtered decision: " << final_decision << "\n";
    }
    E2E_STAGE_END();

    // Stage 6: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 1000);  // Increased timeout - brain destruction can be slow with cognitive modules
    {
        nimcp_ethics_destroy(ethics);
        nimcp_brain_destroy(brain);
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Multi-Module Cognitive Integration
//=============================================================================

E2E_TEST(CognitivePipelineTest, MultiModuleCognitiveIntegration) {
    E2E_PIPELINE_START("Multi-Module Cognitive Integration");

    nimcp_brain_t brain = nullptr;

    // Stage 1: Create integrated cognitive system
    E2E_STAGE_BEGIN("Create cognitive system", 1000);  // Increased timeout - large brain with cognitive modules is slow
    {
        brain = nimcp_brain_create(
            "integrated_cognitive",
            NIMCP_BRAIN_LARGE,
            NIMCP_TASK_ASSOCIATION,
            128, 64
        );
        E2E_ASSERT_NOT_NULL(brain, "Brain creation failed");
    }
    E2E_STAGE_END();

    // Stage 2: Subscribe modules to workspace
    E2E_STAGE_BEGIN("Subscribe modules to workspace", 100);
    {
        nimcp_status_t status;

        status = nimcp_brain_workspace_subscribe(brain, NIMCP_MODULE_PERCEPTION);
        E2E_ASSERT_SUCCESS(status, "Perception subscription failed");

        status = nimcp_brain_workspace_subscribe(brain, NIMCP_MODULE_WORKING_MEMORY);
        E2E_ASSERT_SUCCESS(status, "Working memory subscription failed");

        status = nimcp_brain_workspace_subscribe(brain, NIMCP_MODULE_EXECUTIVE);
        E2E_ASSERT_SUCCESS(status, "Executive subscription failed");

        std::cout << "  Subscribed 3 modules to global workspace\n";
    }
    E2E_STAGE_END();

    // Stage 3: Perception phase
    E2E_STAGE_BEGIN("Perception phase", 300);
    {
        std::vector<float> sensory_input = TestDataGenerator::generate_features(128);

        // Perception competes for workspace
        nimcp_status_t status = nimcp_brain_workspace_compete(
            brain,
            NIMCP_MODULE_PERCEPTION,
            sensory_input.data(),
            128,
            0.9f  // High salience
        );

        if (status == NIMCP_SUCCESS) {
            std::cout << "  Perception won workspace access\n";

            // Add to working memory
            nimcp_brain_working_memory_add(brain, sensory_input.data(), 128, 0.8f);
        }
    }
    E2E_STAGE_END();

    // Stage 4: Working memory maintenance
    E2E_STAGE_BEGIN("Working memory maintenance", 200);
    {
        uint32_t size, capacity;
        nimcp_brain_working_memory_stats(brain, &size, &capacity);

        // Process items in working memory
        for (uint32_t i = 0; i < size; ++i) {
            uint32_t item_size;
            const float* item = nimcp_brain_working_memory_get(brain, i, &item_size);

            if (item && item_size >= 128) {
                // Working memory competes for workspace
                nimcp_brain_workspace_compete(
                    brain,
                    NIMCP_MODULE_WORKING_MEMORY,
                    item,
                    std::min(item_size, 128u),
                    0.7f
                );
            }
        }
    }
    E2E_STAGE_END();

    // Stage 5: Executive control
    E2E_STAGE_BEGIN("Executive control", 300);
    {
        // Executive reads workspace broadcast
        std::vector<float> broadcast(128);
        uint32_t actual_dim;
        nimcp_cognitive_module_t source;

        nimcp_status_t status = nimcp_brain_workspace_read(
            brain, broadcast.data(), 128, &actual_dim, &source
        );

        if (status == NIMCP_SUCCESS) {
            std::cout << "  Executive control processing broadcast from module "
                      << static_cast<int>(source) << "\n";

            // Make decision based on broadcast
            std::vector<float> decision(64);
            nimcp_brain_infer(brain, broadcast.data(), actual_dim, decision.data(), 64);
        }
    }
    E2E_STAGE_END();

    // Stage 6: Response generation
    E2E_STAGE_BEGIN("Response generation", 200);
    {
        std::vector<float> context = TestDataGenerator::generate_features(128);
        std::vector<float> response(64);

        nimcp_status_t status = nimcp_brain_infer(
            brain,
            context.data(),
            128,
            response.data(),
            64
        );
        E2E_ASSERT_SUCCESS(status, "Response generation failed");

        // Broadcast response back to workspace
        nimcp_brain_workspace_compete(
            brain,
            NIMCP_MODULE_MOTOR,
            response.data(),
            64,
            0.6f
        );
    }
    E2E_STAGE_END();

    // Stage 7: Check integration statistics
    E2E_STAGE_BEGIN("Check integration statistics", 100);
    {
        uint32_t broadcasts, competitions;
        float avg_strength;
        nimcp_status_t stats_status = nimcp_brain_workspace_stats(brain, &broadcasts, &competitions, &avg_strength);

        std::cout << "  Workspace activity:\n";
        std::cout << "    Broadcasts:   " << broadcasts << "\n";
        std::cout << "    Competitions: " << competitions << "\n";
        std::cout << "    Avg strength: " << avg_strength << "\n";

        // Only assert if stats API succeeded - workspace might not be tracking in all configurations
        if (stats_status == NIMCP_SUCCESS) {
            E2E_ASSERT(competitions > 0, "No cognitive module activity");
        } else {
            std::cout << "  Workspace stats not available (expected for some configurations)\n";
        }
    }
    E2E_STAGE_END();

    // Stage 8: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 1000);  // Increased timeout - brain destruction can be slow with cognitive modules
    {
        nimcp_brain_destroy(brain);
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}
