/**
 * @file test_cognitive_e2e.cpp
 * @brief End-to-end tests for the cognitive training pipeline
 *
 * WHAT: Test the full cognitive pipeline from training data through module dispatch
 * WHY:  Verify no crashes when simulating the brain_learn_vector cognitive blocks
 *       with representative data from all 9 domains
 * HOW:  Create module structs, run simulated training steps, validate data quality
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <string>
#include <vector>
#include <set>
#include <sstream>

extern "C" {
#include "cognitive/recursive/nimcp_rcog_types.h"
#include "cognitive/recursive/nimcp_rcog_engine.h"
#include "cognitive/ethics/nimcp_ethics.h"
#include "cognitive/memory/nimcp_engram.h"
#include "cognitive/nimcp_theory_of_mind.h"
#include "dragonfly/nimcp_dragonfly.h"
#include "portia/nimcp_portia.h"
#include "cognitive/collective_cognition/nimcp_collective_cognition.h"
#include "cognitive/introspection/nimcp_connectivity_health.h"
}

// Helper: run a Python expression and return stdout
static std::string run_python(const char* expr) {
    char cmd[4096];
    snprintf(cmd, sizeof(cmd),
        "python3 -c \""
        "import sys; sys.path.insert(0, '/home/bbrelin/nimcp/scripts'); "
        "from cognitive_training_data import *; "
        "%s\" 2>/dev/null", expr);
    FILE* fp = popen(cmd, "r");
    if (!fp) return "";
    char buf[8192];
    std::string result;
    while (fgets(buf, sizeof(buf), fp)) result += buf;
    pclose(fp);
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
        result.pop_back();
    return result;
}

// Simulate cognitive dispatch for a single label (mirrors brain_learn_vector logic)
static void simulate_cognitive_dispatch(const char* label, int step) {
    float features[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
    float loss = 100.0f;
    uint32_t num_features = 10;

    if (!label) return;

    // === ToM ===
    // Always update self model (as brain_learn_vector does)
    tom_update_self_model(NULL, features, num_features, label, 0.5f);

    if (strncmp(label, "tom_", 4) == 0 ||
        strstr(label, "perspective") ||
        strstr(label, "belief") ||
        strstr(label, "intention")) {
        tom_observation_t obs;
        memset(&obs, 0, sizeof(obs));
        obs.action_vector = features;
        obs.action_dim = num_features < 256 ? num_features : 256;
        obs.verbal_context = label;
        obs.observed_emotion = (loss < 100.0f) ? TOM_EMOTION_JOY : TOM_EMOTION_SURPRISE;
        tom_observe(NULL, &obs);
    }

    // === RCOG ===
    if (strstr(label, "causal") || strstr(label, "analogy") ||
        strstr(label, "counterfactual") || strstr(label, "rcog_") ||
        strstr(label, "reasoning") || strstr(label, "cognitive")) {
        rcog_goal_t goal = rcog_engine_create_goal(label, RCOG_GOAL_REASONING);
        rcog_process_result_t result;
        memset(&result, 0, sizeof(result));
        rcog_engine_process(NULL, &goal, &result);
    }

    // === Collective ===
    if (step % 100 == 0) {
        collective_cognition_update(NULL);
    }

    // === Dragonfly ===
    if (strstr(label, "dragonfly") || strstr(label, "trajectory") ||
        strstr(label, "tracking") || strstr(label, "intercept")) {
        if (num_features >= 3) {
            dragonfly_detection_t det;
            memset(&det, 0, sizeof(det));
            det.position[0] = features[0];
            det.position[1] = features[1];
            det.position[2] = features[2];
            det.size = 0.05f;
            det.contrast = 0.8f;
            det.motion_speed = 1.0f;
            det.timestamp_us = 1000000 + step;
            det.id = 1;
            dragonfly_process_detection(NULL, &det);
        }
    }

    // === Portia ===
    if (strstr(label, "portia") || strstr(label, "resource") ||
        strstr(label, "platform") || strstr(label, "adaptation")) {
        if (portia_is_initialized()) {
            portia_update();
        }
    }

    // === Ethics ===
    if (strstr(label, "ethics") || strstr(label, "moral") || strstr(label, "dilemma")) {
        action_context_t action;
        memset(&action, 0, sizeof(action));
        action.features = features;
        action.num_features = num_features;
        action.predicted_harm = fminf(loss / 1000.0f, 1.0f);
        if (num_features >= 5) {
            action.fairness_violation = fminf(fabsf(features[0]), 1.0f);
            action.deception_level = fminf(fabsf(features[1]), 1.0f);
            action.autonomy_violation = fminf(fabsf(features[2]), 1.0f);
            action.privacy_violation = fminf(fabsf(features[3]), 1.0f);
            action.consent_violation = fminf(fabsf(features[4]), 1.0f);
        }
        ethics_engine_evaluate_action(NULL, &action);
    }

    // === Introspection ===
    if (strstr(label, "metacog") || strstr(label, "awareness") || strstr(label, "introspect")) {
        connectivity_health_config_t cfg;
        memset(&cfg, 0, sizeof(cfg));
        introspection_assess_connectivity_health(NULL, &cfg);
    }
}

// ============================================================================
// E2E Test: Full Cognitive Pipeline Simulation
// ============================================================================

TEST(CognitiveE2E, FullCognitivePipelineSimulation) {
    // Run 50 simulated training steps with various cognitive labels
    const char* labels[] = {
        "ethics_ethics_trolley",
        "counterfactual_counterfactual_position",
        "causal_causal_multiple_causes",
        "metacog_metacog_learning_awareness",
        "analogy_analogy_functional",
        "rcog_rcog_decomposition",
        "collective_collective_coordination",
        "portia_portia_resource_constrained",
        "dragonfly_dragonfly_parabolic",
        "tom_false_belief",
        "ethics_ethics_integrity",
        "causal_causal_chain_diagnosis",
        "dragonfly_dragonfly_intercept",
        "portia_portia_power_critical",
        "metacog_metacog_uncertainty",
        "analogy_analogy_structural",
        "rcog_rcog_planning",
        "collective_collective_emergence",
        "counterfactual_counterfactual_timing",
        "ethics_ethics_ai_dilemma",
    };
    int num_labels = sizeof(labels) / sizeof(labels[0]);

    for (int step = 0; step < 50; step++) {
        const char* label = labels[step % num_labels];
        simulate_cognitive_dispatch(label, step);
    }
    SUCCEED() << "50 simulated training steps completed without crash";
}

// ============================================================================
// E2E Test: All Domains Exercised
// ============================================================================

TEST(CognitiveE2E, AllDomainsExercised) {
    // One representative label per domain
    const char* domain_labels[] = {
        "ethics_ethics_trolley",
        "counterfactual_counterfactual_position",
        "causal_causal_multiple_causes",
        "metacog_metacog_learning_awareness",
        "analogy_analogy_functional",
        "rcog_rcog_decomposition",
        "collective_collective_coordination",
        "portia_portia_resource_constrained",
        "dragonfly_dragonfly_parabolic",
    };

    for (int i = 0; i < 9; i++) {
        simulate_cognitive_dispatch(domain_labels[i], i);
    }
    SUCCEED() << "All 9 domains exercised without crash";
}

// ============================================================================
// E2E Test: Cognitive With Memory Integration
// ============================================================================

TEST(CognitiveE2E, CognitiveWithMemoryIntegration) {
    engram_system_t* es = engram_system_create();
    ASSERT_NE(es, nullptr);

    // Simulate a training step that also creates an engram
    const char* label = "ethics_ethics_trolley";

    // Cognitive dispatch
    simulate_cognitive_dispatch(label, 0);

    // Engram encoding (as brain_learn_vector does)
    uint32_t neuron_ids[4] = {100, 200, 300, 400};
    float activations[4] = {0.8f, 0.7f, 0.9f, 0.6f};
    emotional_tag_t emotion;
    memset(&emotion, 0, sizeof(emotion));
    emotion.valence = 0.5f;
    emotion.arousal = 0.3f;

    uint64_t eid = engram_encode(es, neuron_ids, activations, 4,
                                  MEMORY_TYPE_EPISODIC, emotion);
    EXPECT_GT(eid, 0u) << "Engram encoding should succeed";
    EXPECT_EQ(es->active_count, 1u);

    engram_system_destroy(es);
}

// ============================================================================
// E2E Test: Label Dispatch Round-Trip From Training Data
// ============================================================================

TEST(CognitiveE2E, LabelDispatchRoundTrip) {
    // Get 20 sample labels from the training data via Python
    std::string out = run_python(
        "data = get_all_cognitive_data();"
        "import random; random.seed(42);"
        "sample = random.sample(data, min(20, len(data)));"
        "print('\\n'.join(d['label'] for d in sample))");
    if (out.empty()) { GTEST_SKIP() << "Python module not importable from test env"; }

    // Parse labels and verify each triggers the right C check
    std::istringstream iss(out);
    std::string label;
    int count = 0;
    while (std::getline(iss, label)) {
        if (label.empty()) continue;
        count++;

        // Verify at least one cognitive trigger matches
        bool matches_any =
            strstr(label.c_str(), "ethics") != NULL ||
            strstr(label.c_str(), "moral") != NULL ||
            strstr(label.c_str(), "dilemma") != NULL ||
            strstr(label.c_str(), "counterfactual") != NULL ||
            strstr(label.c_str(), "imagination") != NULL ||
            strstr(label.c_str(), "causal") != NULL ||
            strstr(label.c_str(), "analogy") != NULL ||
            strstr(label.c_str(), "rcog_") != NULL ||
            strstr(label.c_str(), "reasoning") != NULL ||
            strstr(label.c_str(), "cognitive") != NULL ||
            strstr(label.c_str(), "metacog") != NULL ||
            strstr(label.c_str(), "awareness") != NULL ||
            strstr(label.c_str(), "introspect") != NULL ||
            strstr(label.c_str(), "dragonfly") != NULL ||
            strstr(label.c_str(), "trajectory") != NULL ||
            strstr(label.c_str(), "tracking") != NULL ||
            strstr(label.c_str(), "intercept") != NULL ||
            strstr(label.c_str(), "portia") != NULL ||
            strstr(label.c_str(), "resource") != NULL ||
            strstr(label.c_str(), "platform") != NULL ||
            strstr(label.c_str(), "adaptation") != NULL ||
            strstr(label.c_str(), "collective") != NULL ||
            strncmp(label.c_str(), "tom_", 4) == 0 ||
            strstr(label.c_str(), "perspective") != NULL ||
            strstr(label.c_str(), "belief") != NULL ||
            strstr(label.c_str(), "intention") != NULL;

        EXPECT_TRUE(matches_any)
            << "Label '" << label << "' doesn't match any C-side trigger";
    }
    EXPECT_GE(count, 10) << "Expected at least 10 sample labels";
}

// ============================================================================
// E2E Test: Training Data Quality
// ============================================================================

TEST(CognitiveE2E, TrainingDataQuality) {
    // Comprehensive data quality check
    std::string out = run_python(
        "data = get_all_cognitive_data();"
        "total = len(data);"
        "empty_text = sum(1 for d in data if not d.get('text','').strip());"
        "empty_answer = sum(1 for d in data if not d.get('answer','').strip());"
        "empty_label = sum(1 for d in data if not d.get('label','').strip());"
        "empty_domain = sum(1 for d in data if not d.get('domain','').strip());"
        "short_answer = sum(1 for d in data if len(d.get('answer','')) < 20);"
        "from collections import Counter;"
        "labels = [d['label'] for d in data];"
        "dupes = sum(v-1 for v in Counter(labels).values() if v > 1);"
        "domains = len(set(d['domain'] for d in data));"
        "print(f'{total},{empty_text},{empty_answer},{empty_label},{empty_domain},{short_answer},{dupes},{domains}')");

    if (out.empty()) { GTEST_SKIP() << "Python module not importable from test env"; }

    int total, empty_text, empty_answer, empty_label, empty_domain, short_answer, dupes, domains;
    sscanf(out.c_str(), "%d,%d,%d,%d,%d,%d,%d,%d",
           &total, &empty_text, &empty_answer, &empty_label,
           &empty_domain, &short_answer, &dupes, &domains);

    EXPECT_EQ(total, 185) << "Expected exactly 185 items";
    EXPECT_EQ(empty_text, 0) << "Found items with empty text";
    EXPECT_EQ(empty_answer, 0) << "Found items with empty answer";
    EXPECT_EQ(empty_label, 0) << "Found items with empty label";
    EXPECT_EQ(empty_domain, 0) << "Found items with empty domain";
    EXPECT_EQ(short_answer, 0) << "Found items with answer < 20 chars";
    EXPECT_EQ(dupes, 0) << "Found duplicate labels";
    EXPECT_GE(domains, 9) << "Expected at least 9 domains";
}

// ============================================================================
// E2E Test: Large Scale Simulation
// ============================================================================

TEST(CognitiveE2E, LargeScaleSimulation) {
    // Simulate 200 training steps cycling through all domains
    const char* all_labels[] = {
        "ethics_ethics_trolley", "ethics_ethics_integrity", "ethics_ethics_ai_dilemma",
        "counterfactual_counterfactual_position", "counterfactual_counterfactual_timing",
        "causal_causal_multiple_causes", "causal_causal_chain_diagnosis",
        "metacog_metacog_learning_awareness", "metacog_metacog_uncertainty",
        "analogy_analogy_functional", "analogy_analogy_structural",
        "rcog_rcog_decomposition", "rcog_rcog_planning",
        "collective_collective_coordination", "collective_collective_emergence",
        "portia_portia_resource_constrained", "portia_portia_power_critical",
        "dragonfly_dragonfly_parabolic", "dragonfly_dragonfly_intercept",
        "tom_false_belief",
    };
    int num_labels = sizeof(all_labels) / sizeof(all_labels[0]);

    for (int step = 0; step < 200; step++) {
        simulate_cognitive_dispatch(all_labels[step % num_labels], step);
    }
    SUCCEED() << "200 simulated training steps completed without crash";
}
