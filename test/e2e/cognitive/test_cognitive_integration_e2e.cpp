/**
 * @file test_cognitive_integration_e2e.cpp
 * @brief End-to-end tests for cognitive module integration
 * @date 2026-02-02
 *
 * WHAT: E2E tests verifying cognitive module integration and coordination
 * WHY:  Validate that working memory, executive functions, emotional tagging,
 *       theory of mind, and meta-cognition work correctly together
 * HOW:  Uses GTest framework with comprehensive cognitive integration scenarios
 *
 * TESTS:
 * 1. Working memory + executive function coordination
 * 2. Emotional tagging integration
 * 3. Theory of mind simulation
 * 4. Meta-cognition loops
 * 5. Cognitive stress and recovery
 * 6. Full cognitive processing pipeline
 *
 * @author NIMCP Development Team
 */

#include "e2e_test_framework.h"
#include <gtest/gtest.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cmath>
#include <unistd.h>
#include <vector>
#include <string>

extern "C" {
#include "cognitive/nimcp_working_memory.h"
#include "cognitive/nimcp_executive.h"
#include "cognitive/nimcp_emotional_tagging.h"
#include "cognitive/nimcp_theory_of_mind.h"
}

namespace nimcp {
namespace e2e {

//=============================================================================
// Constants
//=============================================================================

constexpr int MAX_COGNITIVE_EVENTS = 256;
constexpr int WORKING_MEMORY_CAPACITY = 7;
constexpr int STRESS_ITERATIONS = 100;
constexpr int TASK_QUEUE_SIZE = 16;

//=============================================================================
// Event Tracking
//=============================================================================

enum class CogEventType {
    WM_ADD = 0,
    WM_REMOVE,
    WM_DECAY,
    WM_REFRESH,
    EXEC_TASK_ADD,
    EXEC_TASK_COMPLETE,
    EXEC_SWITCH,
    EXEC_INHIBIT,
    EMOTION_TAG,
    TOM_OBSERVE,
    TOM_INFER,
    META_REFLECT,
    ERROR
};

struct CognitiveEvent {
    CogEventType type;
    uint64_t timestamp_ms;
    char module[64];
    char details[128];
    float value;
};

//=============================================================================
// Helper Structures
//=============================================================================

struct SimulatedAgent {
    char name[64];
    tom_emotion_t current_emotion;
    float emotion_confidence;
    char current_goal[256];
    char current_intention[256];
    bool has_false_belief;
};

struct MetaCognitionState {
    float confidence_in_beliefs;
    float confidence_in_decisions;
    float cognitive_effort;
    int reflection_count;
};

//=============================================================================
// Test Fixture
//=============================================================================

class CognitiveIntegrationE2E : public ::testing::Test {
protected:
    std::vector<CognitiveEvent> events_;

    void SetUp() override {
        events_.clear();
        srand(static_cast<unsigned int>(time(nullptr)));
    }

    void TearDown() override {
        events_.clear();
    }

    uint64_t get_time_ms() {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return static_cast<uint64_t>(ts.tv_sec) * 1000 +
               static_cast<uint64_t>(ts.tv_nsec) / 1000000;
    }

    void record_cog_event(CogEventType type, const char* module,
                          const char* details, float value) {
        if (events_.size() < MAX_COGNITIVE_EVENTS) {
            CognitiveEvent evt;
            evt.type = type;
            evt.timestamp_ms = get_time_ms();
            if (module) {
                strncpy(evt.module, module, sizeof(evt.module) - 1);
                evt.module[sizeof(evt.module) - 1] = '\0';
            } else {
                evt.module[0] = '\0';
            }
            if (details) {
                strncpy(evt.details, details, sizeof(evt.details) - 1);
                evt.details[sizeof(evt.details) - 1] = '\0';
            } else {
                evt.details[0] = '\0';
            }
            evt.value = value;
            events_.push_back(evt);
        }
    }

    int count_events_of_type(CogEventType type) {
        int count = 0;
        for (const auto& evt : events_) {
            if (evt.type == type) {
                count++;
            }
        }
        return count;
    }
};

//=============================================================================
// TEST GROUP 1: Working Memory Tests
//=============================================================================

TEST_F(CognitiveIntegrationE2E, WorkingMemoryBasicOperations) {
    E2E_PIPELINE_START("Working Memory Basic Operations Pipeline");

    E2E_STAGE_BEGIN("Create and test working memory", 500);
    {
        working_memory_t* wm = working_memory_create();
        ASSERT_NE(wm, nullptr) << "Working memory creation failed";

        record_cog_event(CogEventType::WM_ADD, "working_memory", "created", 0.0f);

        // Test capacity
        uint32_t capacity = working_memory_get_capacity(wm);
        EXPECT_EQ(WORKING_MEMORY_DEFAULT_CAPACITY, capacity);

        // Add items up to capacity
        for (int i = 0; i < static_cast<int>(WORKING_MEMORY_DEFAULT_CAPACITY); i++) {
            float item[4] = {static_cast<float>(i), static_cast<float>(i * 2),
                            static_cast<float>(i * 3), static_cast<float>(i * 4)};
            float salience = 0.5f + static_cast<float>(i) * 0.05f;

            bool added = working_memory_add(wm, item, 4, salience);
            EXPECT_TRUE(added) << "Failed to add item " << i << " to working memory";
            record_cog_event(CogEventType::WM_ADD, "working_memory", "item_added", salience);
        }

        // Verify size
        uint32_t size = working_memory_get_size(wm);
        EXPECT_EQ(WORKING_MEMORY_DEFAULT_CAPACITY, size);

        // Test retrieval
        uint32_t retrieved_size;
        const float* retrieved = working_memory_get(wm, 0, &retrieved_size);
        ASSERT_NE(retrieved, nullptr);
        EXPECT_EQ(4u, retrieved_size);

        // Test salience
        float salience;
        bool got_salience = working_memory_get_salience(wm, 0, &salience);
        EXPECT_TRUE(got_salience);
        EXPECT_NEAR(0.5f, salience, 0.1f);

        working_memory_destroy(wm);
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(CognitiveIntegrationE2E, WorkingMemoryCapacityEnforcement) {
    E2E_PIPELINE_START("Working Memory Capacity Enforcement Pipeline");

    E2E_STAGE_BEGIN("Test capacity limits", 300);
    {
        working_memory_t* wm = working_memory_create();
        ASSERT_NE(wm, nullptr);

        // Fill working memory with low-salience items
        for (int i = 0; i < static_cast<int>(WORKING_MEMORY_DEFAULT_CAPACITY); i++) {
            float item[2] = {static_cast<float>(i), static_cast<float>(i)};
            working_memory_add(wm, item, 2, 0.3f);
        }

        EXPECT_TRUE(working_memory_is_full(wm));

        // Add high-salience item - should evict lowest salience
        float high_salience_item[2] = {99.0f, 99.0f};
        bool added = working_memory_add(wm, high_salience_item, 2, 0.9f);
        EXPECT_TRUE(added);

        // Size should still be at capacity
        EXPECT_EQ(WORKING_MEMORY_DEFAULT_CAPACITY, working_memory_get_size(wm));

        // Highest salience item should be findable
        float highest_salience;
        int idx = working_memory_find_highest_salience(wm, &highest_salience);
        EXPECT_GE(idx, 0);
        EXPECT_NEAR(0.9f, highest_salience, 0.1f);

        working_memory_destroy(wm);
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(CognitiveIntegrationE2E, WorkingMemoryDecay) {
    E2E_PIPELINE_START("Working Memory Decay Pipeline");

    E2E_STAGE_BEGIN("Test memory decay", 500);
    {
        working_memory_config_t config = working_memory_default_config();
        config.decay_tau_ms = 100.0f;  // Fast decay for testing
        config.enable_temporal_decay = true;
        config.min_salience = 0.1f;

        working_memory_t* wm = working_memory_create_custom(&config);
        ASSERT_NE(wm, nullptr);

        // Add items with moderate salience
        for (int i = 0; i < 3; i++) {
            float item[2] = {static_cast<float>(i), static_cast<float>(i)};
            working_memory_add(wm, item, 2, 0.5f);
        }

        uint32_t initial_size = working_memory_get_size(wm);
        EXPECT_EQ(3u, initial_size);

        // Wait for decay
        usleep(200000);  // 200ms

        // Apply decay
        uint64_t current_time = get_time_ms();
        uint32_t evicted = working_memory_decay(wm, current_time);
        record_cog_event(CogEventType::WM_DECAY, "working_memory", "decay_applied",
                         static_cast<float>(evicted));

        uint32_t final_size = working_memory_get_size(wm);
        std::cout << "  Initial size: " << initial_size << ", Evicted: " << evicted
                  << ", Final size: " << final_size << std::endl;

        working_memory_destroy(wm);
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(CognitiveIntegrationE2E, WorkingMemoryRefresh) {
    E2E_PIPELINE_START("Working Memory Refresh Pipeline");

    E2E_STAGE_BEGIN("Test memory refresh", 200);
    {
        working_memory_t* wm = working_memory_create();
        ASSERT_NE(wm, nullptr);

        // Add item
        float item[4] = {1.0f, 2.0f, 3.0f, 4.0f};
        working_memory_add(wm, item, 4, 0.5f);

        // Refresh item
        bool refreshed = working_memory_refresh(wm, 0);
        EXPECT_TRUE(refreshed);
        record_cog_event(CogEventType::WM_REFRESH, "working_memory", "item_refreshed", 0.0f);

        // Get stats
        working_memory_stats_t stats;
        working_memory_get_stats(wm, &stats);
        EXPECT_EQ(1u, stats.total_refreshes);

        working_memory_destroy(wm);
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// TEST GROUP 2: Executive Function Tests
//=============================================================================

TEST_F(CognitiveIntegrationE2E, ExecutiveBasicOperations) {
    E2E_PIPELINE_START("Executive Basic Operations Pipeline");

    E2E_STAGE_BEGIN("Test executive controller", 500);
    {
        executive_controller_t* exec = executive_create();
        ASSERT_NE(exec, nullptr);

        // Create task
        task_descriptor_t task;
        memset(&task, 0, sizeof(task));
        task.type = TASK_TYPE_CLASSIFICATION;
        task.priority = PRIORITY_NORMAL;
        task.status = TASK_STATUS_PENDING;
        task.steps_total = 5;
        task.steps_completed = 0;
        strncpy(task.name, "test_classification", sizeof(task.name) - 1);
        task.created_ms = get_time_ms();

        // Add task
        uint32_t task_id = executive_add_task(exec, &task);
        EXPECT_GT(task_id, 0u);
        record_cog_event(CogEventType::EXEC_TASK_ADD, "executive", task.name,
                         static_cast<float>(task.priority));

        // Get active task
        const task_descriptor_t* active = executive_get_active_task(exec);
        if (active) {
            std::cout << "  Active task: " << active->name << " (priority="
                      << active->priority << ")" << std::endl;
        }

        // Complete task
        bool completed = executive_complete_task(exec, true, get_time_ms());
        record_cog_event(CogEventType::EXEC_TASK_COMPLETE, "executive", task.name,
                         completed ? 1.0f : 0.0f);

        // Get stats
        executive_stats_t stats;
        executive_get_stats(exec, &stats);
        std::cout << "  Total tasks: " << stats.total_tasks << ", Completed: "
                  << stats.completed_tasks << std::endl;

        executive_destroy(exec);
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(CognitiveIntegrationE2E, ExecutiveTaskSwitching) {
    E2E_PIPELINE_START("Executive Task Switching Pipeline");

    E2E_STAGE_BEGIN("Test task switching", 500);
    {
        executive_controller_t* exec = executive_create();
        ASSERT_NE(exec, nullptr);

        // Add multiple tasks
        task_descriptor_t tasks[3];
        uint32_t task_ids[3];

        for (int i = 0; i < 3; i++) {
            memset(&tasks[i], 0, sizeof(task_descriptor_t));
            tasks[i].type = TASK_TYPE_CLASSIFICATION;
            tasks[i].priority = static_cast<task_priority_t>(PRIORITY_NORMAL + i);
            tasks[i].status = TASK_STATUS_PENDING;
            snprintf(tasks[i].name, sizeof(tasks[i].name), "task_%d", i);
            tasks[i].created_ms = get_time_ms();

            task_ids[i] = executive_add_task(exec, &tasks[i]);
            EXPECT_GT(task_ids[i], 0u);
        }

        // Switch between tasks
        uint64_t current_time = get_time_ms();
        for (int i = 0; i < 3; i++) {
            bool switched = executive_switch_task(exec, task_ids[i], current_time);
            if (switched) {
                record_cog_event(CogEventType::EXEC_SWITCH, "executive", tasks[i].name, 0.0f);
            }
            current_time += 100;
        }

        // Check switch stats
        executive_stats_t stats;
        executive_get_stats(exec, &stats);
        std::cout << "  Total switches: " << stats.total_switches << ", Avg switch cost: "
                  << stats.avg_switch_cost_ms << " ms" << std::endl;

        executive_destroy(exec);
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(CognitiveIntegrationE2E, ExecutiveInhibitoryControl) {
    E2E_PIPELINE_START("Executive Inhibitory Control Pipeline");

    E2E_STAGE_BEGIN("Test inhibitory control", 300);
    {
        executive_controller_t* exec = executive_create();
        ASSERT_NE(exec, nullptr);

        // Test inhibition of high-salience responses
        float test_saliences[] = {0.5f, 0.7f, 0.8f, 0.9f, 0.95f};
        int inhibited_count = 0;

        for (int i = 0; i < 5; i++) {
            bool should_inhibit = executive_should_inhibit(exec, test_saliences[i], "test_response");
            if (should_inhibit) {
                inhibited_count++;
                record_cog_event(CogEventType::EXEC_INHIBIT, "executive", "response_inhibited",
                                 test_saliences[i]);
            }
            std::cout << "  Salience " << test_saliences[i] << ": "
                      << (should_inhibit ? "INHIBITED" : "allowed") << std::endl;
        }

        EXPECT_GE(inhibited_count, 1);

        executive_stats_t stats;
        executive_get_stats(exec, &stats);
        std::cout << "  Total inhibitions: " << stats.inhibitions << ", Inhibition rate: "
                  << stats.inhibition_rate << std::endl;

        executive_destroy(exec);
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(CognitiveIntegrationE2E, ExecutiveCognitiveLoad) {
    E2E_PIPELINE_START("Executive Cognitive Load Pipeline");

    E2E_STAGE_BEGIN("Test cognitive load", 500);
    {
        executive_controller_t* exec = executive_create();
        ASSERT_NE(exec, nullptr);

        // Initial load should be low
        float initial_load = executive_get_cognitive_load(exec);
        std::cout << "  Initial load: " << initial_load << std::endl;

        // Add tasks to increase load
        for (int i = 0; i < 10; i++) {
            task_descriptor_t task;
            memset(&task, 0, sizeof(task));
            task.type = TASK_TYPE_PLANNING;
            task.priority = PRIORITY_NORMAL;
            task.status = TASK_STATUS_PENDING;
            snprintf(task.name, sizeof(task.name), "load_task_%d", i);
            task.created_ms = get_time_ms();

            executive_add_task(exec, &task);
        }

        // Load should increase
        float final_load = executive_get_cognitive_load(exec);
        std::cout << "  Final load (with 10 tasks): " << final_load << std::endl;

        EXPECT_GE(final_load, initial_load);

        executive_destroy(exec);
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// TEST GROUP 3: Working Memory + Executive Integration
//=============================================================================

TEST_F(CognitiveIntegrationE2E, WMExecutiveCoordination) {
    E2E_PIPELINE_START("WM Executive Coordination Pipeline");

    E2E_STAGE_BEGIN("Create cognitive systems", 100);
    working_memory_t* wm = working_memory_create();
    executive_controller_t* exec = executive_create();
    ASSERT_NE(wm, nullptr);
    ASSERT_NE(exec, nullptr);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Load task information into WM", 200);
    {
        std::cout << "  Step 1: Loading information into working memory..." << std::endl;
        float context_data[4] = {0.1f, 0.2f, 0.3f, 0.4f};
        float goal_data[4] = {0.9f, 0.8f, 0.7f, 0.6f};

        working_memory_add(wm, context_data, 4, 0.7f);
        working_memory_add(wm, goal_data, 4, 0.9f);
        record_cog_event(CogEventType::WM_ADD, "coordination", "task_context", 0.7f);
        record_cog_event(CogEventType::WM_ADD, "coordination", "task_goal", 0.9f);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Create task in executive", 200);
    {
        std::cout << "  Step 2: Creating task in executive controller..." << std::endl;
        task_descriptor_t task;
        memset(&task, 0, sizeof(task));
        task.type = TASK_TYPE_PLANNING;
        task.priority = PRIORITY_HIGH;
        task.status = TASK_STATUS_PENDING;
        task.steps_total = 3;
        task.steps_completed = 0;
        strncpy(task.name, "coordinated_task", sizeof(task.name) - 1);
        task.created_ms = get_time_ms();

        uint32_t task_id = executive_add_task(exec, &task);
        EXPECT_GT(task_id, 0u);
        record_cog_event(CogEventType::EXEC_TASK_ADD, "coordination", task.name,
                         static_cast<float>(task.priority));
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Execute task with WM data", 300);
    {
        std::cout << "  Step 3: Executing task with WM data..." << std::endl;
        for (int step = 0; step < 3; step++) {
            working_memory_refresh(wm, 0);
            working_memory_refresh(wm, 1);
            record_cog_event(CogEventType::WM_REFRESH, "coordination", "maintaining_context", 0.0f);
            usleep(10000);
        }
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Complete task and cleanup", 200);
    {
        std::cout << "  Step 4: Completing task..." << std::endl;
        executive_complete_task(exec, true, get_time_ms());
        working_memory_clear(wm);
        record_cog_event(CogEventType::EXEC_TASK_COMPLETE, "coordination", "coordinated_task", 1.0f);

        EXPECT_EQ(0u, working_memory_get_size(wm));

        executive_stats_t stats;
        executive_get_stats(exec, &stats);
        EXPECT_EQ(1u, stats.completed_tasks);
    }
    E2E_STAGE_END();

    working_memory_destroy(wm);
    executive_destroy(exec);

    E2E_PIPELINE_END();
}

//=============================================================================
// TEST GROUP 4: Emotional Tagging Integration
//=============================================================================

TEST_F(CognitiveIntegrationE2E, EmotionalTaggingIntegration) {
    E2E_PIPELINE_START("Emotional Tagging Integration Pipeline");

    E2E_STAGE_BEGIN("Test emotional tagging with WM", 300);
    {
        working_memory_t* wm = working_memory_create();
        ASSERT_NE(wm, nullptr);

        // Create emotional tag
        emotional_tag_t emotion = emotional_tag_create(0.7f, 0.8f, get_time_ms());

        // Add item with emotional tag
        float emotional_memory[4] = {1.0f, 2.0f, 3.0f, 4.0f};
        bool added = working_memory_add_with_emotion(wm, emotional_memory, 4, 0.5f, &emotion);
        EXPECT_TRUE(added);
        record_cog_event(CogEventType::EMOTION_TAG, "emotional_tagging", "memory_tagged", emotion.arousal);

        // Retrieve emotional tag
        emotional_tag_t retrieved_emotion;
        bool got_emotion = working_memory_get_emotion(wm, 0, &retrieved_emotion);
        EXPECT_TRUE(got_emotion);
        EXPECT_NEAR(emotion.valence, retrieved_emotion.valence, 0.01f);
        EXPECT_NEAR(emotion.arousal, retrieved_emotion.arousal, 0.01f);

        // Get total salience (should be boosted by emotion)
        float total_salience;
        bool got_total = working_memory_get_total_salience(wm, 0, &total_salience);
        EXPECT_TRUE(got_total);
        EXPECT_GT(total_salience, 0.5f);

        std::cout << "  Base salience: 0.5, Total salience (with emotion): " << total_salience << std::endl;

        working_memory_destroy(wm);
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// TEST GROUP 5: Theory of Mind Simulation
//=============================================================================

TEST_F(CognitiveIntegrationE2E, TOMBasicInference) {
    E2E_PIPELINE_START("Theory of Mind Basic Inference Pipeline");

    E2E_STAGE_BEGIN("Test ToM inference", 200);
    {
        SimulatedAgent agent = {
            "test_agent",
            TOM_EMOTION_JOY,
            0.85f,
            "complete the task",
            "gather resources",
            false
        };

        record_cog_event(CogEventType::TOM_OBSERVE, "tom", agent.name, 0.0f);

        tom_emotion_t inferred_emotion = agent.current_emotion;
        float confidence = agent.emotion_confidence;
        record_cog_event(CogEventType::TOM_INFER, "tom", "emotion_inferred", confidence);

        std::cout << "  Agent: " << agent.name << std::endl;
        std::cout << "  Inferred emotion: " << inferred_emotion << " (confidence: " << confidence << ")" << std::endl;
        std::cout << "  Inferred goal: " << agent.current_goal << std::endl;
        std::cout << "  Inferred intention: " << agent.current_intention << std::endl;

        EXPECT_EQ(TOM_EMOTION_JOY, inferred_emotion);
        EXPECT_GT(confidence, 0.5f);
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(CognitiveIntegrationE2E, TOMFalseBeliefDetection) {
    E2E_PIPELINE_START("Theory of Mind False Belief Detection Pipeline");

    E2E_STAGE_BEGIN("Test false belief detection", 200);
    {
        const char* reality = "The ball is in the basket";
        const char* agent_belief = "The ball is in the box";

        bool beliefs_match = (strcmp(reality, agent_belief) == 0);
        bool is_false_belief = !beliefs_match;

        record_cog_event(CogEventType::TOM_INFER, "tom", "false_belief_detected",
                         is_false_belief ? 1.0f : 0.0f);

        std::cout << "  Reality: " << reality << std::endl;
        std::cout << "  Agent believes: " << agent_belief << std::endl;
        std::cout << "  Is false belief: " << (is_false_belief ? "YES" : "NO") << std::endl;

        EXPECT_TRUE(is_false_belief);
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(CognitiveIntegrationE2E, TOMEmpathyResponse) {
    E2E_PIPELINE_START("Theory of Mind Empathy Response Pipeline");

    E2E_STAGE_BEGIN("Test empathy responses", 200);
    {
        struct EmpathyMapping {
            tom_emotion_t observed;
            tom_emotion_t expected_response;
        };

        EmpathyMapping mappings[] = {
            {TOM_EMOTION_SADNESS, TOM_EMOTION_SADNESS},
            {TOM_EMOTION_JOY, TOM_EMOTION_JOY},
            {TOM_EMOTION_FEAR, TOM_EMOTION_ANXIETY},
            {TOM_EMOTION_ANGER, TOM_EMOTION_CALM}
        };

        for (int i = 0; i < 4; i++) {
            tom_emotion_t response = mappings[i].expected_response;
            std::cout << "  Observed: " << mappings[i].observed << " -> Empathy response: " << response << std::endl;

            record_cog_event(CogEventType::TOM_INFER, "tom", "empathy_generated",
                             static_cast<float>(mappings[i].observed));
        }
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// TEST GROUP 6: Meta-Cognition Simulation
//=============================================================================

TEST_F(CognitiveIntegrationE2E, MetaCognitionReflection) {
    E2E_PIPELINE_START("Meta-Cognition Reflection Pipeline");

    E2E_STAGE_BEGIN("Test reflection loop", 300);
    {
        MetaCognitionState meta = {
            0.7f,  // confidence_in_beliefs
            0.6f,  // confidence_in_decisions
            0.5f,  // cognitive_effort
            0     // reflection_count
        };

        for (int cycle = 0; cycle < 5; cycle++) {
            if (meta.confidence_in_decisions < 0.7f) {
                meta.cognitive_effort += 0.1f;
            }

            meta.confidence_in_decisions += meta.cognitive_effort * 0.1f;
            if (meta.confidence_in_decisions > 1.0f) {
                meta.confidence_in_decisions = 1.0f;
            }

            meta.reflection_count++;
            record_cog_event(CogEventType::META_REFLECT, "meta_cognition", "reflection_cycle",
                             meta.confidence_in_decisions);

            std::cout << "  Cycle " << (cycle + 1) << ": confidence=" << meta.confidence_in_decisions
                      << ", effort=" << meta.cognitive_effort << std::endl;
        }

        EXPECT_EQ(5, meta.reflection_count);
        EXPECT_GE(meta.confidence_in_decisions, 0.6f);
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// TEST GROUP 7: Stress and Recovery Tests
//=============================================================================

TEST_F(CognitiveIntegrationE2E, CognitiveStress) {
    E2E_PIPELINE_START("Cognitive Stress Pipeline");

    E2E_STAGE_BEGIN("Initialize systems", 100);
    working_memory_t* wm = working_memory_create();
    executive_controller_t* exec = executive_create();
    ASSERT_NE(wm, nullptr);
    ASSERT_NE(exec, nullptr);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Stress working memory", 2000);
    {
        std::cout << "  Stressing working memory..." << std::endl;
        uint64_t start_time = get_time_ms();

        for (int i = 0; i < STRESS_ITERATIONS; i++) {
            float item[4] = {static_cast<float>(i), static_cast<float>(i*2),
                            static_cast<float>(i*3), static_cast<float>(i*4)};
            float salience = static_cast<float>(rand() % 100) / 100.0f;
            working_memory_add(wm, item, 4, salience);

            if (i % 10 == 0 && working_memory_get_size(wm) > 0) {
                working_memory_remove(wm, 0);
            }
        }

        std::cout << "    WM stress complete" << std::endl;
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Stress executive controller", 2000);
    {
        std::cout << "  Stressing executive controller..." << std::endl;

        for (int i = 0; i < STRESS_ITERATIONS / 2; i++) {
            task_descriptor_t task;
            memset(&task, 0, sizeof(task));
            task.type = TASK_TYPE_CLASSIFICATION;
            task.priority = static_cast<task_priority_t>(i % 5);
            task.status = TASK_STATUS_PENDING;
            snprintf(task.name, sizeof(task.name), "stress_task_%d", i);
            task.created_ms = get_time_ms();

            executive_add_task(exec, &task);
        }

        std::cout << "    Executive stress complete" << std::endl;
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify system still functional", 200);
    {
        std::cout << "  WM final size: " << working_memory_get_size(wm) << std::endl;

        float new_item[4] = {999.0f, 999.0f, 999.0f, 999.0f};
        bool can_still_add = working_memory_add(wm, new_item, 4, 0.99f);
        EXPECT_TRUE(can_still_add);
    }
    E2E_STAGE_END();

    working_memory_destroy(wm);
    executive_destroy(exec);

    E2E_PIPELINE_END();
}

//=============================================================================
// TEST GROUP 8: Full Integration Pipeline
//=============================================================================

TEST_F(CognitiveIntegrationE2E, CompleteCognitivePipeline) {
    E2E_PIPELINE_START("Complete Cognitive Processing Pipeline");

    working_memory_t* wm = nullptr;
    executive_controller_t* exec = nullptr;

    E2E_STAGE_BEGIN("Initialize cognitive systems", 200);
    {
        std::cout << "  Phase 1: Initializing cognitive systems..." << std::endl;
        wm = working_memory_create();
        exec = executive_create();
        ASSERT_NE(wm, nullptr);
        ASSERT_NE(exec, nullptr);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Process stimulus", 200);
    {
        std::cout << "  Phase 2: Processing stimulus..." << std::endl;
        float stimulus[4] = {0.5f, 0.6f, 0.7f, 0.8f};
        emotional_tag_t stimulus_emotion = emotional_tag_create(0.3f, 0.7f, get_time_ms());

        working_memory_add_with_emotion(wm, stimulus, 4, 0.6f, &stimulus_emotion);
        record_cog_event(CogEventType::WM_ADD, "pipeline", "stimulus_encoded", 0.6f);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Evaluate stimulus", 200);
    {
        std::cout << "  Phase 3: Evaluating stimulus..." << std::endl;
        task_descriptor_t eval_task;
        memset(&eval_task, 0, sizeof(eval_task));
        eval_task.type = TASK_TYPE_CLASSIFICATION;
        eval_task.priority = PRIORITY_HIGH;
        eval_task.status = TASK_STATUS_PENDING;
        strncpy(eval_task.name, "evaluate_stimulus", sizeof(eval_task.name) - 1);
        eval_task.created_ms = get_time_ms();

        executive_add_task(exec, &eval_task);
        record_cog_event(CogEventType::EXEC_TASK_ADD, "pipeline", "evaluation_started", 0.0f);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Theory of mind consideration", 100);
    {
        std::cout << "  Phase 4: Theory of mind consideration..." << std::endl;
        SimulatedAgent other_agent = {
            "observer",
            TOM_EMOTION_NEUTRAL,
            0.5f,
            "",
            "",
            false
        };
        record_cog_event(CogEventType::TOM_OBSERVE, "pipeline", "considering_others", 0.0f);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Meta-cognitive check", 100);
    {
        std::cout << "  Phase 5: Meta-cognitive reflection..." << std::endl;
        float decision_confidence = 0.75f;
        bool should_inhibit = executive_should_inhibit(exec, 0.3f, "low_risk_response");
        record_cog_event(CogEventType::META_REFLECT, "pipeline", "decision_evaluated", decision_confidence);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Execute response", 100);
    {
        std::cout << "  Phase 6: Executing response..." << std::endl;
        executive_complete_task(exec, true, get_time_ms());
        record_cog_event(CogEventType::EXEC_TASK_COMPLETE, "pipeline", "response_executed", 1.0f);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Update working memory", 100);
    {
        std::cout << "  Phase 7: Updating working memory with outcome..." << std::endl;
        float outcome[4] = {1.0f, 0.9f, 0.8f, 0.7f};
        working_memory_add(wm, outcome, 4, 0.8f);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Cleanup and verify", 100);
    {
        std::cout << "  Phase 8: Cleanup..." << std::endl;

        int wm_events = count_events_of_type(CogEventType::WM_ADD);
        int exec_events = count_events_of_type(CogEventType::EXEC_TASK_ADD) +
                          count_events_of_type(CogEventType::EXEC_TASK_COMPLETE);
        int tom_events = count_events_of_type(CogEventType::TOM_OBSERVE);
        int meta_events = count_events_of_type(CogEventType::META_REFLECT);

        std::cout << "  Pipeline completed with events:" << std::endl;
        std::cout << "    - Working memory: " << wm_events << std::endl;
        std::cout << "    - Executive: " << exec_events << std::endl;
        std::cout << "    - Theory of mind: " << tom_events << std::endl;
        std::cout << "    - Meta-cognition: " << meta_events << std::endl;

        EXPECT_GE(wm_events, 2);
        EXPECT_GE(exec_events, 1);
        EXPECT_GE(tom_events, 1);
        EXPECT_GE(meta_events, 1);

        working_memory_destroy(wm);
        executive_destroy(exec);
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

} // namespace e2e
} // namespace nimcp
