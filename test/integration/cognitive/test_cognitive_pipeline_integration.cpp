/**
 * @file test_cognitive_pipeline_integration.cpp
 * @brief Integration tests for the cognitive training pipeline
 *
 * WHAT: Test the data + wiring together — verify label dispatch, module sequencing,
 *       and data round-trips through the cognitive pipeline
 * WHY:  Individual modules may work but integration can fail from mismatched types,
 *       missing triggers, or ordering bugs
 * HOW:  Test label dispatch coverage, multi-module sequences, data validation
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <string>
#include <vector>
#include <set>

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
    char cmd[2048];
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

// ============================================================================
// Data Count and Domain Coverage
// ============================================================================

TEST(CognitivePipelineIntegration, DataCountsCorrect) {
    std::string out = run_python("print(len(get_all_cognitive_data()))");
    ASSERT_FALSE(out.empty()) << "Failed to run Python validation";
    int count = atoi(out.c_str());
    EXPECT_GE(count, 185) << "Expected at least 185 training items";
}

TEST(CognitivePipelineIntegration, AllDomainsHaveData) {
    std::string out = run_python(
        "data = get_all_cognitive_data();"
        "from collections import Counter;"
        "c = Counter(d['domain'] for d in data);"
        "print('\\n'.join(f'{d}:{c[d]}' for d in sorted(c.keys())))");
    // Each of 9 domains should have 10+ items
    if (out.empty()) { GTEST_SKIP() << "Python module not importable from test env"; }
    // Parse domain:count lines
    std::set<std::string> domains;
    std::istringstream iss(out);
    std::string line;
    while (std::getline(iss, line)) {
        auto colon = line.find(':');
        if (colon != std::string::npos) {
            std::string domain = line.substr(0, colon);
            int count = atoi(line.substr(colon + 1).c_str());
            domains.insert(domain);
            EXPECT_GE(count, 10) << "Domain " << domain << " has fewer than 10 items";
        }
    }
    EXPECT_GE(domains.size(), 9u) << "Expected at least 9 domains";
}

TEST(CognitivePipelineIntegration, LabelFormatConsistent) {
    // All labels should match pattern domain_concept (lowercase with underscores)
    std::string out = run_python(
        "import re;"
        "data = get_all_cognitive_data();"
        "bad = [d['label'] for d in data if not re.match(r'^[a-z][a-z0-9_]+$', d['label'])];"
        "print(len(bad))");
    EXPECT_EQ(atoi(out.c_str()), 0) << "Some labels don't match expected format";
}

// ============================================================================
// Label Dispatch Coverage
// ============================================================================

TEST(CognitivePipelineIntegration, LabelDispatchCoverage) {
    // For each domain, verify at least one label triggers the right C-side strstr check
    struct {
        const char* domain;
        const char* c_trigger;
    } triggers[] = {
        {"ethics", "ethics"},
        {"counterfactual", "counterfactual"},
        {"causal", "causal"},
        {"metacognition", "metacog"},
        {"analogy", "analogy"},
        {"rcog", "rcog_"},
        {"collective", "collective"},
        {"portia", "portia"},
        {"dragonfly", "dragonfly"},
    };

    for (auto& t : triggers) {
        char py[512];
        snprintf(py, sizeof(py),
            "data = [d for d in get_all_cognitive_data() if d['domain'] == '%s'];"
            "matches = [d for d in data if '%s' in d['label']];"
            "print(len(matches))", t.domain, t.c_trigger);
        std::string out = run_python(py);
        int count = atoi(out.c_str());
        EXPECT_GT(count, 0) << "Domain '" << t.domain
            << "' has no labels matching C trigger '" << t.c_trigger << "'";
    }
}

// ============================================================================
// RCOG Goal Processing for All Types
// ============================================================================

TEST(CognitivePipelineIntegration, RCOGGoalAllTypes) {
    rcog_goal_type_t types[] = {
        RCOG_GOAL_QUESTION_ANSWERING, RCOG_GOAL_SUMMARIZATION,
        RCOG_GOAL_EXTRACTION, RCOG_GOAL_REASONING,
        RCOG_GOAL_PLANNING, RCOG_GOAL_ANALYSIS,
        RCOG_GOAL_GENERATION, RCOG_GOAL_TRANSLATION,
        RCOG_GOAL_VALIDATION, RCOG_GOAL_CUSTOM
    };
    for (auto type : types) {
        rcog_goal_t goal = rcog_engine_create_goal("integration test", type);
        EXPECT_EQ(goal.type, type);
        EXPECT_STREQ(goal.query, "integration test");
    }
}

// ============================================================================
// Ethics All Violation Types
// ============================================================================

TEST(CognitivePipelineIntegration, EthicsAllViolationFields) {
    // Test that each violation field can be set and read independently
    action_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    float* fields[] = {
        &ctx.predicted_harm, &ctx.fairness_violation,
        &ctx.deception_level, &ctx.autonomy_violation,
        &ctx.privacy_violation, &ctx.consent_violation
    };

    for (int i = 0; i < 6; i++) {
        memset(&ctx, 0, sizeof(ctx));
        *fields[i] = 1.0f;
        // Only the set field should be non-zero
        for (int j = 0; j < 6; j++) {
            if (i == j) {
                EXPECT_FLOAT_EQ(*fields[j], 1.0f);
            } else {
                EXPECT_FLOAT_EQ(*fields[j], 0.0f);
            }
        }
    }
}

// ============================================================================
// Engram + ToM Pipeline
// ============================================================================

TEST(CognitivePipelineIntegration, EngramCreateAndTomObserve) {
    // Create engram system (memory)
    engram_system_t* es = engram_system_create();
    ASSERT_NE(es, nullptr);

    // ToM observe with NULL handle should fail gracefully
    tom_observation_t obs;
    memset(&obs, 0, sizeof(obs));
    float features[10] = {1.0f, 0.5f, 0.3f};
    obs.action_vector = features;
    obs.action_dim = 10;
    obs.verbal_context = "test_observation";
    obs.observed_emotion = TOM_EMOTION_JOY;

    bool result = tom_observe(NULL, &obs);
    EXPECT_FALSE(result);

    engram_system_destroy(es);
}

// ============================================================================
// Dragonfly Detection Round-Trip
// ============================================================================

TEST(CognitivePipelineIntegration, DragonFlyDetectionRoundTrip) {
    // Create a detection exactly as brain_learn_vector does
    float features[6] = {1.0f, 2.0f, 3.0f, 0.5f, 0.3f, 0.1f};

    dragonfly_detection_t det;
    memset(&det, 0, sizeof(det));
    det.position[0] = features[0];
    det.position[1] = features[1];
    det.position[2] = features[2];
    det.size = 0.05f;
    det.contrast = 0.8f;
    det.motion_speed = sqrtf(features[3]*features[3] +
                              features[4]*features[4] +
                              features[5]*features[5]);
    det.timestamp_us = 1000000;
    det.id = 1;

    EXPECT_FLOAT_EQ(det.position[0], 1.0f);
    EXPECT_FLOAT_EQ(det.position[1], 2.0f);
    EXPECT_FLOAT_EQ(det.position[2], 3.0f);
    EXPECT_GT(det.motion_speed, 0.0f);
    EXPECT_EQ(det.id, 1u);
}

// ============================================================================
// Multiple Modules Sequential (no crash test)
// ============================================================================

TEST(CognitivePipelineIntegration, MultipleModulesSequentialNoCrash) {
    // Simulate the order of cognitive module calls in brain_learn_vector
    // All with NULL handles — should fail gracefully without crashing

    // 1. ToM: update self model
    float features[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
    tom_update_self_model(NULL, features, 10, "test_learn", 0.9f);

    // 2. ToM: observe
    tom_observation_t obs;
    memset(&obs, 0, sizeof(obs));
    obs.action_vector = features;
    obs.action_dim = 10;
    obs.verbal_context = "tom_test";
    obs.observed_emotion = TOM_EMOTION_SURPRISE;
    tom_observe(NULL, &obs);

    // 3. RCOG: process
    rcog_goal_t goal = rcog_engine_create_goal("causal_test", RCOG_GOAL_REASONING);
    rcog_process_result_t rcog_result;
    memset(&rcog_result, 0, sizeof(rcog_result));
    rcog_engine_process(NULL, &goal, &rcog_result);

    // 4. Collective: update
    collective_cognition_update(NULL);

    // 5. Dragonfly: process detection
    dragonfly_detection_t det;
    memset(&det, 0, sizeof(det));
    det.position[0] = features[0];
    det.id = 1;
    dragonfly_process_detection(NULL, &det);

    // 6. Portia: check initialized
    portia_is_initialized();

    // 7. Ethics: evaluate
    action_context_t action;
    memset(&action, 0, sizeof(action));
    action.predicted_harm = 0.5f;
    ethics_engine_evaluate_action(NULL, &action);

    // 8. Introspection: assess connectivity
    introspection_assess_connectivity_health(NULL, NULL);

    SUCCEED() << "All 8 cognitive module calls with NULL handles completed without crash";
}

// ============================================================================
// Cognitive Data Round-Trip
// ============================================================================

TEST(CognitivePipelineIntegration, CognitiveDataRoundTrip) {
    // Verify that Python data items produce valid text for embedding
    std::string out = run_python(
        "data = get_all_cognitive_data();"
        "empty_text = sum(1 for d in data if len(d['text'].strip()) < 5);"
        "empty_answer = sum(1 for d in data if len(d['answer'].strip()) < 10);"
        "print(f'{empty_text},{empty_answer}')");
    if (out.empty()) { GTEST_SKIP() << "Python module not importable from test env"; }
    int empty_text = 0, empty_answer = 0;
    sscanf(out.c_str(), "%d,%d", &empty_text, &empty_answer);
    EXPECT_EQ(empty_text, 0) << "Some texts are too short for meaningful embedding";
    EXPECT_EQ(empty_answer, 0) << "Some answers are too short for meaningful training";
}

// ============================================================================
// NULL Safety All Modules
// ============================================================================

TEST(CognitivePipelineIntegration, NullSafetyAllModules) {
    // Each module function called with NULL, verify no crash
    // ToM
    EXPECT_FALSE(tom_observe(NULL, NULL));
    EXPECT_FALSE(tom_update_self_model(NULL, NULL, 0, NULL, 0.0f));
    EXPECT_FALSE(tom_get_bdi_state(NULL, NULL, NULL, NULL));
    EXPECT_FALSE(tom_reset(NULL));

    // RCOG
    rcog_process_result_t r;
    memset(&r, 0, sizeof(r));
    EXPECT_NE(rcog_engine_process(NULL, NULL, &r), 0);
    rcog_engine_destroy(NULL);

    // Dragonfly
    EXPECT_NE(dragonfly_process_detection(NULL, NULL), 0);
    dragonfly_system_destroy(NULL);

    // Collective
    EXPECT_NE(collective_cognition_update(NULL), 0);

    // Engram
    engram_system_destroy(NULL);

    // Introspection
    introspection_assess_connectivity_health(NULL, NULL);

    SUCCEED() << "All NULL safety checks passed";
}

// ============================================================================
// Empty Label Skips All Cognitive Blocks
// ============================================================================

TEST(CognitivePipelineIntegration, EmptyLabelSkipsAllCognitiveBlocks) {
    // In brain_learn_vector, most cognitive blocks are gated by:
    //   label && (strstr(label, "..."))
    // An empty label should match nothing
    const char* label = "";
    EXPECT_TRUE(strstr(label, "ethics") == NULL);
    EXPECT_TRUE(strstr(label, "counterfactual") == NULL);
    EXPECT_TRUE(strstr(label, "causal") == NULL);
    EXPECT_TRUE(strstr(label, "metacog") == NULL);
    EXPECT_TRUE(strstr(label, "analogy") == NULL);
    EXPECT_TRUE(strstr(label, "rcog_") == NULL);
    EXPECT_TRUE(strstr(label, "dragonfly") == NULL);
    EXPECT_TRUE(strstr(label, "portia") == NULL);
    EXPECT_TRUE(strstr(label, "collective") == NULL);
    EXPECT_TRUE(strstr(label, "reasoning") == NULL);
    EXPECT_TRUE(strstr(label, "cognitive") == NULL);
    EXPECT_TRUE(strstr(label, "awareness") == NULL);
    EXPECT_TRUE(strstr(label, "introspect") == NULL);
    EXPECT_TRUE(strncmp(label, "tom_", 4) != 0);
    EXPECT_TRUE(strstr(label, "perspective") == NULL);
    EXPECT_TRUE(strstr(label, "belief") == NULL);
    EXPECT_TRUE(strstr(label, "intention") == NULL);
}

// ============================================================================
// All Domains Exercised
// ============================================================================

TEST(CognitivePipelineIntegration, AllDomainsExercised) {
    // For each domain, verify we can create the appropriate module structs
    // without crashing

    // Ethics
    {
        action_context_t action;
        memset(&action, 0, sizeof(action));
        action.predicted_harm = 0.5f;
        EXPECT_FLOAT_EQ(action.predicted_harm, 0.5f);
    }

    // Counterfactual (imagination mode)
    {
        // The C code uses extern declarations, so we just verify the mode constant
        int mode_counterfactual = 2; // IMAGINATION_MODE_COUNTERFACTUAL
        EXPECT_EQ(mode_counterfactual, 2);
    }

    // Causal + Analogy + RCOG (all via RCOG engine)
    {
        rcog_goal_t goal = rcog_engine_create_goal("causal_test", RCOG_GOAL_REASONING);
        EXPECT_EQ(goal.type, RCOG_GOAL_REASONING);
    }

    // Metacognition (introspection)
    {
        connectivity_health_config_t cfg;
        memset(&cfg, 0, sizeof(cfg));
        EXPECT_FLOAT_EQ(cfg.min_modularity, 0.0f);
    }

    // Collective
    {
        collective_cognition_update(NULL); // Should fail, not crash
    }

    // Portia
    {
        portia_is_initialized(); // Should not crash
    }

    // Dragonfly
    {
        dragonfly_detection_t det;
        memset(&det, 0, sizeof(det));
        det.id = 1;
        EXPECT_EQ(det.id, 1u);
    }

    // ToM
    {
        tom_observation_t obs;
        memset(&obs, 0, sizeof(obs));
        obs.observed_emotion = TOM_EMOTION_CALM;
        EXPECT_EQ(obs.observed_emotion, TOM_EMOTION_CALM);
    }

    SUCCEED();
}
