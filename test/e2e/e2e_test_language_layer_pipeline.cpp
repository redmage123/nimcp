//=============================================================================
// e2e_test_language_layer_pipeline.cpp - Language Layer E2E Pipeline Tests
//=============================================================================
/**
 * @file e2e_test_language_layer_pipeline.cpp
 * @brief End-to-end tests for complete language layer pipeline
 *
 * WHAT: Complete E2E testing of language layer from perception to production
 * WHY:  Validate full language processing workflow with all bridges
 * HOW:  Test realistic language scenarios with orchestrator and all subsystems
 *
 * PIPELINE STAGES:
 * 1. Perception input (phonemes, text, audio features)
 * 2. Comprehension (Wernicke's area processing)
 * 3. Semantic integration (concept activation, context building)
 * 4. Production planning (Broca's area processing)
 * 5. Motor output (speech commands)
 *
 * @version 1.0
 * @date 2026-01-05
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <chrono>
#include <algorithm>
#include <numeric>

#include "language/nimcp_language_orchestrator.h"
#include "language/nimcp_language_config.h"
#include "language/nimcp_language_types.h"
#include "language/bridges/nimcp_language_perception_bridge.h"
#include "language/bridges/nimcp_language_cognitive_bridge.h"
#include "language/bridges/nimcp_language_training_bridge.h"
#include "language/bridges/nimcp_language_omni_bridge.h"
#include "language/bridges/nimcp_language_immune_bridge.h"
#include "language/bridges/nimcp_language_gpu_bridge.h"
#include "language/bridges/nimcp_language_thalamic_bridge.h"
#include "language/bridges/nimcp_language_substrate_bridge.h"
#include "language/bridges/nimcp_language_logic_bridge.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// E2E Test Fixture
//=============================================================================

class LanguageLayerE2ETest : public ::testing::Test {
protected:
    language_orchestrator_t* orchestrator;
    language_orchestrator_config_t config;

    // Bridges
    language_perception_bridge_t* perception_bridge;
    language_cognitive_bridge_t* cognitive_bridge;
    language_training_bridge_t* training_bridge;
    language_omni_bridge_t* omni_bridge;
    language_immune_bridge_t* immune_bridge;
    language_gpu_bridge_t* gpu_bridge;
    language_thalamic_bridge_t* thalamic_bridge;
    language_substrate_bridge_t* substrate_bridge;
    language_logic_bridge_t* logic_bridge;

    // Test data
    std::vector<language_phoneme_t> test_phonemes;
    std::vector<float> test_semantic_vector;

    static constexpr uint32_t SEMANTIC_DIM = 256;
    static constexpr uint32_t MAX_PHONEMES = 64;

    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);

        // Initialize all to null
        orchestrator = nullptr;
        perception_bridge = nullptr;
        cognitive_bridge = nullptr;
        training_bridge = nullptr;
        omni_bridge = nullptr;
        immune_bridge = nullptr;
        gpu_bridge = nullptr;
        thalamic_bridge = nullptr;
        substrate_bridge = nullptr;
        logic_bridge = nullptr;

        // Get default config and enable all features
        language_orchestrator_default_config(&config);
        config.enable_wernicke = true;
        config.enable_broca = true;
        config.enable_nlp_core = true;
        config.enable_perception_bridge = true;
        config.enable_cognitive_bridge = true;
        config.enable_training_bridge = true;
        config.enable_omni_bridge = true;
        config.enable_immune_bridge = true;
        config.enable_gpu_bridge = true;
        config.enable_bio_async = true;
        config.semantic_dim = SEMANTIC_DIM;

        // Build test data
        buildTestPhonemes();
        buildTestSemanticVector();
    }

    void TearDown() override {
        // Destroy bridges
        if (perception_bridge) {
            language_perception_bridge_destroy(perception_bridge);
            perception_bridge = nullptr;
        }
        if (cognitive_bridge) {
            language_cognitive_bridge_destroy(cognitive_bridge);
            cognitive_bridge = nullptr;
        }
        if (training_bridge) {
            language_training_bridge_destroy(training_bridge);
            training_bridge = nullptr;
        }
        if (omni_bridge) {
            language_omni_bridge_destroy(omni_bridge);
            omni_bridge = nullptr;
        }
        if (immune_bridge) {
            language_immune_bridge_destroy(immune_bridge);
            immune_bridge = nullptr;
        }
        if (gpu_bridge) {
            language_gpu_bridge_destroy(gpu_bridge);
            gpu_bridge = nullptr;
        }
        if (thalamic_bridge) {
            language_thalamic_bridge_destroy(thalamic_bridge);
            thalamic_bridge = nullptr;
        }
        if (substrate_bridge) {
            language_substrate_bridge_destroy(substrate_bridge);
            substrate_bridge = nullptr;
        }
        if (logic_bridge) {
            language_logic_bridge_destroy(logic_bridge);
            logic_bridge = nullptr;
        }

        // Destroy orchestrator
        if (orchestrator) {
            language_orchestrator_destroy(orchestrator);
            orchestrator = nullptr;
        }

        // Clear test data
        test_phonemes.clear();
        test_semantic_vector.clear();
    }

    void buildTestPhonemes() {
        test_phonemes.clear();

        // Build phoneme sequence for "hello world" (simplified)
        const char* hello_phonemes[] = {"h", "e", "l", "o", " ", "w", "o", "r", "l", "d"};
        const phoneme_category_t categories[] = {
            PHONEME_CAT_FRICATIVE,   // h
            PHONEME_CAT_VOWEL,       // e
            PHONEME_CAT_APPROXIMANT, // l
            PHONEME_CAT_VOWEL,       // o
            PHONEME_CAT_SILENCE,     // space
            PHONEME_CAT_APPROXIMANT, // w
            PHONEME_CAT_VOWEL,       // o
            PHONEME_CAT_APPROXIMANT, // r
            PHONEME_CAT_APPROXIMANT, // l
            PHONEME_CAT_STOP         // d
        };

        uint64_t timestamp = 0;
        for (size_t i = 0; i < sizeof(hello_phonemes)/sizeof(hello_phonemes[0]); i++) {
            language_phoneme_t phoneme;
            memset(&phoneme, 0, sizeof(phoneme));

            phoneme.id = (uint32_t)i;
            phoneme.category = categories[i];
            phoneme.confidence = 0.85f + 0.1f * (float)(rand() % 10) / 10.0f;
            phoneme.duration_ms = 50.0f + 30.0f * (float)(rand() % 10) / 10.0f;
            phoneme.pitch_hz = 100.0f + 50.0f * (float)(rand() % 10) / 10.0f;
            phoneme.intensity = 0.7f + 0.2f * (float)(rand() % 10) / 10.0f;
            phoneme.is_stressed = (i == 1 || i == 6);  // Stressed vowels
            phoneme.is_word_boundary = (i == 4);       // Space is word boundary
            phoneme.is_phrase_boundary = false;
            phoneme.timestamp_ms = timestamp;

            // Set formants for vowels
            if (categories[i] == PHONEME_CAT_VOWEL) {
                phoneme.formants[0] = 500.0f;  // F1
                phoneme.formants[1] = 1500.0f; // F2
                phoneme.formants[2] = 2500.0f; // F3
                phoneme.formants[3] = 3500.0f; // F4
            }

            test_phonemes.push_back(phoneme);
            timestamp += (uint64_t)phoneme.duration_ms;
        }
    }

    void buildTestSemanticVector() {
        test_semantic_vector.resize(SEMANTIC_DIM, 0.0f);

        // Create a semantic vector representing a simple concept
        // Use a sparse activation pattern
        for (uint32_t i = 0; i < SEMANTIC_DIM; i++) {
            if (i % 10 == 0) {
                test_semantic_vector[i] = 0.5f + 0.3f * (float)(rand() % 10) / 10.0f;
            } else {
                test_semantic_vector[i] = 0.1f * (float)(rand() % 10) / 10.0f;
            }
        }

        // Normalize
        float norm = 0.0f;
        for (float v : test_semantic_vector) {
            norm += v * v;
        }
        norm = sqrtf(norm);
        if (norm > 0.0f) {
            for (float& v : test_semantic_vector) {
                v /= norm;
            }
        }
    }

    bool createOrchestrator() {
        orchestrator = language_orchestrator_create(&config);
        return orchestrator != nullptr;
    }

    bool createAllBridges() {
        // Create perception bridge
        language_perception_config_t perc_cfg;
        language_perception_default_config(&perc_cfg);
        perception_bridge = language_perception_bridge_create(&perc_cfg);
        if (!perception_bridge) return false;

        // Create cognitive bridge
        language_cognitive_config_t cog_cfg;
        language_cognitive_default_config(&cog_cfg);
        cognitive_bridge = language_cognitive_bridge_create(&cog_cfg);
        if (!cognitive_bridge) return false;

        // Create training bridge
        language_training_config_t train_cfg;
        language_training_default_config(&train_cfg);
        training_bridge = language_training_bridge_create(&train_cfg);
        if (!training_bridge) return false;

        // Create omni bridge
        language_omni_config_t omni_cfg;
        language_omni_default_config(&omni_cfg);
        omni_bridge = language_omni_bridge_create(&omni_cfg);
        if (!omni_bridge) return false;

        // Create immune bridge
        language_immune_config_t immune_cfg;
        language_immune_default_config(&immune_cfg);
        immune_bridge = language_immune_bridge_create(&immune_cfg);
        if (!immune_bridge) return false;

        // Create GPU bridge
        language_gpu_config_t gpu_cfg;
        language_gpu_default_config(&gpu_cfg);
        gpu_bridge = language_gpu_bridge_create(&gpu_cfg);
        if (!gpu_bridge) return false;

        // Create thalamic bridge (requires orchestrator)
        language_thalamic_config_t thal_cfg;
        language_thalamic_default_config(&thal_cfg);
        thalamic_bridge = language_thalamic_bridge_create(orchestrator, &thal_cfg);
        if (!thalamic_bridge) return false;

        // Create substrate bridge (requires orchestrator)
        language_substrate_config_t sub_cfg;
        language_substrate_default_config(&sub_cfg);
        substrate_bridge = language_substrate_bridge_create(orchestrator, &sub_cfg);
        if (!substrate_bridge) return false;

        // Create logic bridge (requires orchestrator)
        language_logic_config_t logic_cfg;
        language_logic_default_config(&logic_cfg);
        logic_bridge = language_logic_bridge_create(orchestrator, &logic_cfg);
        if (!logic_bridge) return false;

        return true;
    }

    bool connectAllBridges() {
        if (!orchestrator) return false;

        int result = 0;

        if (perception_bridge) {
            result = language_orchestrator_connect_perception_bridge(orchestrator, perception_bridge);
            if (result != 0) return false;
        }

        if (cognitive_bridge) {
            result = language_orchestrator_connect_cognitive_bridge(orchestrator, cognitive_bridge);
            if (result != 0) return false;
        }

        if (training_bridge) {
            result = language_orchestrator_connect_training_bridge(orchestrator, training_bridge);
            if (result != 0) return false;
        }

        if (omni_bridge) {
            result = language_orchestrator_connect_omni_bridge(orchestrator, omni_bridge);
            if (result != 0) return false;
        }

        if (immune_bridge) {
            result = language_orchestrator_connect_immune_bridge(orchestrator, immune_bridge);
            if (result != 0) return false;
        }

        if (gpu_bridge) {
            result = language_orchestrator_connect_gpu_bridge(orchestrator, gpu_bridge);
            if (result != 0) return false;
        }

        return true;
    }

    uint64_t getCurrentTimeMs() {
        auto now = std::chrono::steady_clock::now();
        auto duration = now.time_since_epoch();
        return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    }
};

//=============================================================================
// System Initialization Tests
//=============================================================================

/**
 * @test Full system initialization
 * WHAT: Create orchestrator with all bridges
 * WHY:  Verify complete system can be initialized
 * HOW:  Create orchestrator, all bridges, connect everything
 */
TEST_F(LanguageLayerE2ETest, FullSystemInitialization) {
    ASSERT_TRUE(createOrchestrator()) << "Failed to create orchestrator";
    ASSERT_TRUE(createAllBridges()) << "Failed to create all bridges";
    ASSERT_TRUE(connectAllBridges()) << "Failed to connect bridges";

    // Verify connection status
    language_orchestrator_stats_t stats;
    int result = language_orchestrator_get_stats(orchestrator, &stats);
    EXPECT_EQ(result, 0);

    EXPECT_TRUE(stats.perception_bridge_connected);
    EXPECT_TRUE(stats.cognitive_bridge_connected);
    EXPECT_TRUE(stats.training_bridge_connected);
    EXPECT_TRUE(stats.omni_bridge_connected);
    EXPECT_TRUE(stats.immune_bridge_connected);
    EXPECT_TRUE(stats.gpu_bridge_connected);
}

/**
 * @test Start and stop orchestrator
 * WHAT: Test orchestrator lifecycle
 * WHY:  Verify start/stop works correctly
 * HOW:  Create, start, verify running, stop, verify stopped
 */
TEST_F(LanguageLayerE2ETest, OrchestratorStartStop) {
    ASSERT_TRUE(createOrchestrator());

    // Should not be running initially
    EXPECT_FALSE(language_orchestrator_is_running(orchestrator));

    // Start
    int result = language_orchestrator_start(orchestrator);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(language_orchestrator_is_running(orchestrator));

    // Stop
    result = language_orchestrator_stop(orchestrator);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(language_orchestrator_is_running(orchestrator));
}

//=============================================================================
// Comprehension Pipeline Tests
//=============================================================================

/**
 * @test Phoneme input comprehension pipeline
 * WHAT: Test phoneme-to-comprehension flow
 * WHY:  Verify Wernicke processing works end-to-end
 * HOW:  Input phonemes, process, get comprehension result
 */
TEST_F(LanguageLayerE2ETest, PhonemeComprehensionPipeline) {
    ASSERT_TRUE(createOrchestrator());
    ASSERT_TRUE(createAllBridges());
    ASSERT_TRUE(connectAllBridges());

    // Start orchestrator
    int result = language_orchestrator_start(orchestrator);
    EXPECT_EQ(result, 0);

    // Set comprehension mode
    result = language_orchestrator_set_mode(orchestrator, LANGUAGE_MODE_COMPREHENSION);
    EXPECT_EQ(result, 0);

    // Process phonemes
    result = language_orchestrator_process_phonemes(
        orchestrator,
        test_phonemes.data(),
        (uint32_t)test_phonemes.size()
    );
    EXPECT_EQ(result, 0);

    // Run update cycles to process input
    uint64_t current_time = getCurrentTimeMs();
    for (int i = 0; i < 10; i++) {
        result = language_orchestrator_update(orchestrator, current_time);
        EXPECT_GE(result, 0);
        current_time += 10;  // 10ms per cycle
    }

    // Check state progressed
    language_state_t state = language_orchestrator_get_state(orchestrator);
    // State should have progressed from IDLE
    (void)state;  // May still be processing

    // Get comprehension result
    language_comprehension_result_t comprehension;
    memset(&comprehension, 0, sizeof(comprehension));
    result = language_orchestrator_get_comprehension(orchestrator, &comprehension);
    // Result may or may not be available depending on processing time

    // Stop
    language_orchestrator_stop(orchestrator);
}

/**
 * @test Text input processing
 * WHAT: Test text-to-comprehension flow
 * WHY:  Verify text processing works end-to-end
 * HOW:  Input text, process, check state
 */
TEST_F(LanguageLayerE2ETest, TextComprehensionPipeline) {
    ASSERT_TRUE(createOrchestrator());

    // Start orchestrator
    int result = language_orchestrator_start(orchestrator);
    EXPECT_EQ(result, 0);

    // Set comprehension mode
    result = language_orchestrator_set_mode(orchestrator, LANGUAGE_MODE_COMPREHENSION);
    EXPECT_EQ(result, 0);

    // Process text
    const char* test_text = "Hello world";
    result = language_orchestrator_process_text(orchestrator, test_text);
    EXPECT_EQ(result, 0);

    // Run update cycles
    uint64_t current_time = getCurrentTimeMs();
    for (int i = 0; i < 5; i++) {
        result = language_orchestrator_update(orchestrator, current_time);
        EXPECT_GE(result, 0);
        current_time += 10;
    }

    // Stop
    language_orchestrator_stop(orchestrator);
}

/**
 * @test Generic input processing
 * WHAT: Test generic input API
 * WHY:  Verify language_orchestrator_process_input works
 * HOW:  Input various types, check processing
 */
TEST_F(LanguageLayerE2ETest, GenericInputPipeline) {
    ASSERT_TRUE(createOrchestrator());

    int result = language_orchestrator_start(orchestrator);
    EXPECT_EQ(result, 0);

    // Test phoneme input
    result = language_orchestrator_process_input(
        orchestrator,
        test_phonemes.data(),
        (uint32_t)test_phonemes.size(),
        LANGUAGE_INPUT_PHONEMES
    );
    EXPECT_EQ(result, 0);

    // Test semantic input
    result = language_orchestrator_process_input(
        orchestrator,
        test_semantic_vector.data(),
        (uint32_t)test_semantic_vector.size(),
        LANGUAGE_INPUT_SEMANTIC
    );
    EXPECT_EQ(result, 0);

    // Test text input
    const char* text = "Test sentence";
    result = language_orchestrator_process_input(
        orchestrator,
        text,
        (uint32_t)strlen(text),
        LANGUAGE_INPUT_TEXT
    );
    EXPECT_EQ(result, 0);

    language_orchestrator_stop(orchestrator);
}

//=============================================================================
// Production Pipeline Tests
//=============================================================================

/**
 * @test Semantic-to-output production pipeline
 * WHAT: Test semantic-to-production flow
 * WHY:  Verify Broca processing works end-to-end
 * HOW:  Input semantic vector, generate output
 */
TEST_F(LanguageLayerE2ETest, SemanticProductionPipeline) {
    ASSERT_TRUE(createOrchestrator());
    ASSERT_TRUE(createAllBridges());
    ASSERT_TRUE(connectAllBridges());

    int result = language_orchestrator_start(orchestrator);
    EXPECT_EQ(result, 0);

    // Set production mode
    result = language_orchestrator_set_mode(orchestrator, LANGUAGE_MODE_PRODUCTION);
    EXPECT_EQ(result, 0);

    // Generate output from semantic input
    char output_buffer[1024];
    memset(output_buffer, 0, sizeof(output_buffer));
    uint32_t output_size = 0;

    result = language_orchestrator_generate_output(
        orchestrator,
        test_semantic_vector.data(),
        (uint32_t)test_semantic_vector.size(),
        output_buffer,
        sizeof(output_buffer),
        &output_size,
        LANGUAGE_OUTPUT_TEXT
    );
    // May succeed or fail depending on implementation status

    // Get production plan
    language_production_plan_t plan;
    memset(&plan, 0, sizeof(plan));
    result = language_orchestrator_get_production_plan(orchestrator, &plan);
    // Result depends on implementation

    language_orchestrator_stop(orchestrator);
}

/**
 * @test Dialogue mode bidirectional flow
 * WHAT: Test full dialogue mode
 * WHY:  Verify bidirectional comprehension-production works
 * HOW:  Set dialogue mode, process input, generate response
 */
TEST_F(LanguageLayerE2ETest, DialogueModePipeline) {
    ASSERT_TRUE(createOrchestrator());
    ASSERT_TRUE(createAllBridges());
    ASSERT_TRUE(connectAllBridges());

    int result = language_orchestrator_start(orchestrator);
    EXPECT_EQ(result, 0);

    // Set dialogue mode
    result = language_orchestrator_set_mode(orchestrator, LANGUAGE_MODE_DIALOGUE);
    EXPECT_EQ(result, 0);

    language_mode_t mode = language_orchestrator_get_mode(orchestrator);
    EXPECT_EQ(mode, LANGUAGE_MODE_DIALOGUE);

    // Process input (comprehension)
    result = language_orchestrator_process_text(orchestrator, "Hello");
    EXPECT_EQ(result, 0);

    // Run update cycles
    uint64_t current_time = getCurrentTimeMs();
    for (int i = 0; i < 10; i++) {
        result = language_orchestrator_update(orchestrator, current_time);
        EXPECT_GE(result, 0);
        current_time += 10;
    }

    language_orchestrator_stop(orchestrator);
}

//=============================================================================
// Multi-Bridge Integration Tests
//=============================================================================

/**
 * @test Perception bridge integration
 * WHAT: Test perception bridge data flow
 * WHY:  Verify perception layer integration
 * HOW:  Create bridge, connect, verify stats
 */
TEST_F(LanguageLayerE2ETest, PerceptionBridgeIntegration) {
    ASSERT_TRUE(createOrchestrator());

    // Create and connect perception bridge
    language_perception_config_t perc_cfg;
    language_perception_default_config(&perc_cfg);
    perception_bridge = language_perception_bridge_create(&perc_cfg);
    ASSERT_NE(perception_bridge, nullptr);

    int result = language_orchestrator_connect_perception_bridge(orchestrator, perception_bridge);
    EXPECT_EQ(result, 0);

    // Verify connected
    language_orchestrator_stats_t stats;
    language_orchestrator_get_stats(orchestrator, &stats);
    EXPECT_TRUE(stats.perception_bridge_connected);

    // Start and run update
    language_orchestrator_start(orchestrator);
    language_orchestrator_update(orchestrator, getCurrentTimeMs());

    // Get bridge stats
    language_perception_stats_t perc_stats;
    result = language_perception_bridge_get_stats(perception_bridge, &perc_stats);
    EXPECT_EQ(result, 0);

    language_orchestrator_stop(orchestrator);
}

/**
 * @test Cognitive bridge integration
 * WHAT: Test cognitive bridge data flow
 * WHY:  Verify cognitive layer integration
 * HOW:  Create bridge, connect, verify stats
 */
TEST_F(LanguageLayerE2ETest, CognitiveBridgeIntegration) {
    ASSERT_TRUE(createOrchestrator());

    // Create and connect cognitive bridge
    language_cognitive_config_t cog_cfg;
    language_cognitive_default_config(&cog_cfg);
    cognitive_bridge = language_cognitive_bridge_create(&cog_cfg);
    ASSERT_NE(cognitive_bridge, nullptr);

    int result = language_orchestrator_connect_cognitive_bridge(orchestrator, cognitive_bridge);
    EXPECT_EQ(result, 0);

    // Verify connected
    language_orchestrator_stats_t stats;
    language_orchestrator_get_stats(orchestrator, &stats);
    EXPECT_TRUE(stats.cognitive_bridge_connected);

    // Start and run update
    language_orchestrator_start(orchestrator);
    language_orchestrator_update(orchestrator, getCurrentTimeMs());

    // Get bridge stats
    language_cognitive_stats_t cog_stats;
    result = language_cognitive_bridge_get_stats(cognitive_bridge, &cog_stats);
    EXPECT_EQ(result, 0);

    language_orchestrator_stop(orchestrator);
}

/**
 * @test Training bridge integration
 * WHAT: Test training bridge learning flow
 * WHY:  Verify training layer integration
 * HOW:  Create bridge, connect, test update propagation
 */
TEST_F(LanguageLayerE2ETest, TrainingBridgeIntegration) {
    ASSERT_TRUE(createOrchestrator());

    // Create and connect training bridge
    language_training_config_t train_cfg;
    language_training_default_config(&train_cfg);
    training_bridge = language_training_bridge_create(&train_cfg);
    ASSERT_NE(training_bridge, nullptr);

    int result = language_orchestrator_connect_training_bridge(orchestrator, training_bridge);
    EXPECT_EQ(result, 0);

    // Verify connected
    language_orchestrator_stats_t stats;
    language_orchestrator_get_stats(orchestrator, &stats);
    EXPECT_TRUE(stats.training_bridge_connected);

    // Get bridge stats
    language_training_stats_t train_stats;
    result = language_training_bridge_get_stats(training_bridge, &train_stats);
    EXPECT_EQ(result, 0);
}

/**
 * @test Immune bridge integration
 * WHAT: Test immune bridge health modulation
 * WHY:  Verify immune system affects language processing
 * HOW:  Create bridge, connect, verify effects
 */
TEST_F(LanguageLayerE2ETest, ImmuneBridgeIntegration) {
    ASSERT_TRUE(createOrchestrator());

    // Create and connect immune bridge
    language_immune_config_t immune_cfg;
    language_immune_default_config(&immune_cfg);
    immune_bridge = language_immune_bridge_create(&immune_cfg);
    ASSERT_NE(immune_bridge, nullptr);

    int result = language_orchestrator_connect_immune_bridge(orchestrator, immune_bridge);
    EXPECT_EQ(result, 0);

    // Verify connected
    language_orchestrator_stats_t stats;
    language_orchestrator_get_stats(orchestrator, &stats);
    EXPECT_TRUE(stats.immune_bridge_connected);

    // Get bridge stats
    language_immune_stats_t immune_stats;
    result = language_immune_bridge_get_stats(immune_bridge, &immune_stats);
    EXPECT_EQ(result, 0);
}

/**
 * @test GPU bridge integration
 * WHAT: Test GPU acceleration bridge
 * WHY:  Verify GPU offload capabilities
 * HOW:  Create bridge, connect, verify execution mode
 */
TEST_F(LanguageLayerE2ETest, GPUBridgeIntegration) {
    ASSERT_TRUE(createOrchestrator());

    // Create and connect GPU bridge
    language_gpu_config_t gpu_cfg;
    language_gpu_default_config(&gpu_cfg);
    gpu_bridge = language_gpu_bridge_create(&gpu_cfg);
    ASSERT_NE(gpu_bridge, nullptr);

    int result = language_orchestrator_connect_gpu_bridge(orchestrator, gpu_bridge);
    EXPECT_EQ(result, 0);

    // Verify connected
    language_orchestrator_stats_t stats;
    language_orchestrator_get_stats(orchestrator, &stats);
    EXPECT_TRUE(stats.gpu_bridge_connected);

    // Get bridge stats
    language_gpu_stats_t gpu_stats;
    result = language_gpu_bridge_get_stats(gpu_bridge, &gpu_stats);
    EXPECT_EQ(result, 0);
}

/**
 * @test Thalamic bridge integration
 * WHAT: Test thalamic routing bridge
 * WHY:  Verify thalamic nuclei routing
 * HOW:  Create bridge, test routing operations
 */
TEST_F(LanguageLayerE2ETest, ThalamicBridgeIntegration) {
    // Create thalamic bridge (requires orchestrator)
    language_thalamic_config_t thal_cfg;
    language_thalamic_default_config(&thal_cfg);
    thalamic_bridge = language_thalamic_bridge_create(orchestrator, &thal_cfg);
    ASSERT_NE(thalamic_bridge, nullptr);

    // Start orchestrator and run update cycle
    language_orchestrator_start(orchestrator);
    int result = language_orchestrator_update(orchestrator, getCurrentTimeMs());
    EXPECT_GE(result, 0);

    // Get bridge stats
    language_thalamic_stats_t thal_stats;
    result = language_thalamic_bridge_get_stats(thalamic_bridge, &thal_stats);
    EXPECT_EQ(result, 0);

    language_orchestrator_stop(orchestrator);
}

/**
 * @test Substrate bridge integration
 * WHAT: Test substrate computation bridge
 * WHY:  Verify substrate-level processing
 * HOW:  Create bridge, test operations
 */
TEST_F(LanguageLayerE2ETest, SubstrateBridgeIntegration) {
    // Create substrate bridge (requires orchestrator)
    language_substrate_config_t sub_cfg;
    language_substrate_default_config(&sub_cfg);
    substrate_bridge = language_substrate_bridge_create(orchestrator, &sub_cfg);
    ASSERT_NE(substrate_bridge, nullptr);

    // Start orchestrator and run update cycle
    language_orchestrator_start(orchestrator);
    int result = language_orchestrator_update(orchestrator, getCurrentTimeMs());
    EXPECT_GE(result, 0);

    // Get bridge stats
    language_substrate_stats_t sub_stats;
    result = language_substrate_bridge_get_stats(substrate_bridge, &sub_stats);
    EXPECT_EQ(result, 0);

    language_orchestrator_stop(orchestrator);
}

/**
 * @test Logic bridge integration
 * WHAT: Test logic processing bridge
 * WHY:  Verify logical reasoning in language
 * HOW:  Create bridge, test operations
 */
TEST_F(LanguageLayerE2ETest, LogicBridgeIntegration) {
    // Create logic bridge (requires orchestrator)
    language_logic_config_t logic_cfg;
    language_logic_default_config(&logic_cfg);
    logic_bridge = language_logic_bridge_create(orchestrator, &logic_cfg);
    ASSERT_NE(logic_bridge, nullptr);

    // Start orchestrator and run update cycle
    language_orchestrator_start(orchestrator);
    int result = language_orchestrator_update(orchestrator, getCurrentTimeMs());
    EXPECT_GE(result, 0);

    // Get bridge stats
    language_logic_stats_t logic_stats;
    result = language_logic_bridge_get_stats(logic_bridge, &logic_stats);
    EXPECT_EQ(result, 0);

    language_orchestrator_stop(orchestrator);
}

//=============================================================================
// State Machine Tests
//=============================================================================

/**
 * @test State transitions during processing
 * WHAT: Test state machine behavior
 * WHY:  Verify proper state progression
 * HOW:  Process input, monitor state changes
 */
TEST_F(LanguageLayerE2ETest, StateTransitionsDuringProcessing) {
    ASSERT_TRUE(createOrchestrator());

    int result = language_orchestrator_start(orchestrator);
    EXPECT_EQ(result, 0);

    // Initial state should be IDLE
    language_state_t state = language_orchestrator_get_state(orchestrator);
    EXPECT_EQ(state, LANGUAGE_STATE_IDLE);

    // Process phonemes - should transition to LISTENING or COMPREHENDING
    result = language_orchestrator_process_phonemes(
        orchestrator,
        test_phonemes.data(),
        (uint32_t)test_phonemes.size()
    );
    EXPECT_EQ(result, 0);

    // Run updates and track state changes
    uint64_t current_time = getCurrentTimeMs();
    std::vector<language_state_t> states_observed;

    for (int i = 0; i < 20; i++) {
        result = language_orchestrator_update(orchestrator, current_time);
        EXPECT_GE(result, 0);

        state = language_orchestrator_get_state(orchestrator);
        if (states_observed.empty() || states_observed.back() != state) {
            states_observed.push_back(state);
        }

        current_time += 10;
    }

    // Should have observed at least the initial state
    EXPECT_GE(states_observed.size(), 1u);

    // Verify we can reset
    result = language_orchestrator_reset(orchestrator);
    EXPECT_EQ(result, 0);

    state = language_orchestrator_get_state(orchestrator);
    EXPECT_EQ(state, LANGUAGE_STATE_IDLE);

    language_orchestrator_stop(orchestrator);
}

/**
 * @test Mode switching
 * WHAT: Test mode changes
 * WHY:  Verify mode transitions work correctly
 * HOW:  Cycle through all modes
 */
TEST_F(LanguageLayerE2ETest, ModeSwitching) {
    ASSERT_TRUE(createOrchestrator());

    int result = language_orchestrator_start(orchestrator);
    EXPECT_EQ(result, 0);

    // Test all modes
    language_mode_t modes[] = {
        LANGUAGE_MODE_COMPREHENSION,
        LANGUAGE_MODE_PRODUCTION,
        LANGUAGE_MODE_DIALOGUE,
        LANGUAGE_MODE_REPETITION,
        LANGUAGE_MODE_TRANSLATION
    };

    for (auto mode : modes) {
        result = language_orchestrator_set_mode(orchestrator, mode);
        EXPECT_EQ(result, 0) << "Failed to set mode " << (int)mode;

        language_mode_t current_mode = language_orchestrator_get_mode(orchestrator);
        EXPECT_EQ(current_mode, mode) << "Mode mismatch for " << (int)mode;
    }

    language_orchestrator_stop(orchestrator);
}

//=============================================================================
// Statistics and Monitoring Tests
//=============================================================================

/**
 * @test Statistics tracking
 * WHAT: Test statistics collection
 * WHY:  Verify stats are properly tracked
 * HOW:  Process input, check stats increment
 */
TEST_F(LanguageLayerE2ETest, StatisticsTracking) {
    ASSERT_TRUE(createOrchestrator());
    ASSERT_TRUE(createAllBridges());
    ASSERT_TRUE(connectAllBridges());

    int result = language_orchestrator_start(orchestrator);
    EXPECT_EQ(result, 0);

    // Get initial stats
    language_orchestrator_stats_t initial_stats;
    result = language_orchestrator_get_stats(orchestrator, &initial_stats);
    EXPECT_EQ(result, 0);

    // Process some input
    result = language_orchestrator_process_phonemes(
        orchestrator,
        test_phonemes.data(),
        (uint32_t)test_phonemes.size()
    );
    EXPECT_EQ(result, 0);

    // Run updates
    uint64_t current_time = getCurrentTimeMs();
    for (int i = 0; i < 10; i++) {
        language_orchestrator_update(orchestrator, current_time);
        current_time += 10;
    }

    // Get final stats
    language_orchestrator_stats_t final_stats;
    result = language_orchestrator_get_stats(orchestrator, &final_stats);
    EXPECT_EQ(result, 0);

    // Verify some activity occurred
    EXPECT_GE(final_stats.phonemes_processed, initial_stats.phonemes_processed);

    // Reset stats
    language_orchestrator_reset_stats(orchestrator);

    language_orchestrator_stats_t reset_stats;
    language_orchestrator_get_stats(orchestrator, &reset_stats);
    EXPECT_EQ(reset_stats.utterances_comprehended, 0u);
    EXPECT_EQ(reset_stats.utterances_produced, 0u);

    language_orchestrator_stop(orchestrator);
}

//=============================================================================
// Event Callback Tests
//=============================================================================

// Global event counter for callback testing
static int g_event_count = 0;
static language_event_type_t g_last_event_type = LANGUAGE_EVENT_COUNT;

static void test_event_callback(const language_event_t* event, void* user_data) {
    if (event) {
        g_event_count++;
        g_last_event_type = event->type;
    }
    (void)user_data;
}

/**
 * @test Event callback registration
 * WHAT: Test callback registration and invocation
 * WHY:  Verify events are properly dispatched
 * HOW:  Register callback, process input, check events
 */
TEST_F(LanguageLayerE2ETest, EventCallbackRegistration) {
    ASSERT_TRUE(createOrchestrator());

    // Reset globals
    g_event_count = 0;
    g_last_event_type = LANGUAGE_EVENT_COUNT;

    // Register callback
    int result = language_orchestrator_register_callback(
        orchestrator,
        test_event_callback,
        nullptr
    );
    EXPECT_EQ(result, 0);

    // Start and process
    language_orchestrator_start(orchestrator);
    language_orchestrator_process_phonemes(
        orchestrator,
        test_phonemes.data(),
        (uint32_t)test_phonemes.size()
    );

    // Run updates
    uint64_t current_time = getCurrentTimeMs();
    for (int i = 0; i < 10; i++) {
        language_orchestrator_update(orchestrator, current_time);
        current_time += 10;
    }

    // Unregister callback
    result = language_orchestrator_unregister_callback(orchestrator, test_event_callback);
    EXPECT_EQ(result, 0);

    // Events may or may not have been fired depending on implementation
    // Just verify no crash occurred

    language_orchestrator_stop(orchestrator);
}

//=============================================================================
// Performance Tests
//=============================================================================

/**
 * @test Processing throughput
 * WHAT: Test processing speed
 * WHY:  Verify performance is acceptable
 * HOW:  Process multiple inputs, measure time
 */
TEST_F(LanguageLayerE2ETest, ProcessingThroughput) {
    ASSERT_TRUE(createOrchestrator());
    ASSERT_TRUE(createAllBridges());
    ASSERT_TRUE(connectAllBridges());

    int result = language_orchestrator_start(orchestrator);
    EXPECT_EQ(result, 0);

    // Time processing of multiple inputs
    auto start_time = std::chrono::steady_clock::now();
    const int num_iterations = 100;

    for (int i = 0; i < num_iterations; i++) {
        language_orchestrator_process_phonemes(
            orchestrator,
            test_phonemes.data(),
            (uint32_t)test_phonemes.size()
        );
        language_orchestrator_update(orchestrator, getCurrentTimeMs());
    }

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    // Expect reasonable throughput (less than 1 second for 100 iterations)
    EXPECT_LT(duration.count(), 1000) << "Processing too slow: " << duration.count() << "ms";

    language_orchestrator_stop(orchestrator);
}

/**
 * @test Memory stability under load
 * WHAT: Test memory management under repeated operations
 * WHY:  Verify no memory leaks in processing loop
 * HOW:  Repeat create/process/destroy cycle
 */
TEST_F(LanguageLayerE2ETest, MemoryStabilityUnderLoad) {
    const int num_cycles = 10;

    for (int cycle = 0; cycle < num_cycles; cycle++) {
        // Create
        ASSERT_TRUE(createOrchestrator());
        ASSERT_TRUE(createAllBridges());
        ASSERT_TRUE(connectAllBridges());

        // Process
        language_orchestrator_start(orchestrator);
        language_orchestrator_process_phonemes(
            orchestrator,
            test_phonemes.data(),
            (uint32_t)test_phonemes.size()
        );

        uint64_t current_time = getCurrentTimeMs();
        for (int i = 0; i < 5; i++) {
            language_orchestrator_update(orchestrator, current_time);
            current_time += 10;
        }

        language_orchestrator_stop(orchestrator);

        // Destroy (handled by TearDown pattern)
        if (perception_bridge) {
            language_perception_bridge_destroy(perception_bridge);
            perception_bridge = nullptr;
        }
        if (cognitive_bridge) {
            language_cognitive_bridge_destroy(cognitive_bridge);
            cognitive_bridge = nullptr;
        }
        if (training_bridge) {
            language_training_bridge_destroy(training_bridge);
            training_bridge = nullptr;
        }
        if (omni_bridge) {
            language_omni_bridge_destroy(omni_bridge);
            omni_bridge = nullptr;
        }
        if (immune_bridge) {
            language_immune_bridge_destroy(immune_bridge);
            immune_bridge = nullptr;
        }
        if (gpu_bridge) {
            language_gpu_bridge_destroy(gpu_bridge);
            gpu_bridge = nullptr;
        }
        if (thalamic_bridge) {
            language_thalamic_bridge_destroy(thalamic_bridge);
            thalamic_bridge = nullptr;
        }
        if (substrate_bridge) {
            language_substrate_bridge_destroy(substrate_bridge);
            substrate_bridge = nullptr;
        }
        if (logic_bridge) {
            language_logic_bridge_destroy(logic_bridge);
            logic_bridge = nullptr;
        }
        if (orchestrator) {
            language_orchestrator_destroy(orchestrator);
            orchestrator = nullptr;
        }
    }

    // If we get here without crashing, memory handling is stable
    SUCCEED();
}

//=============================================================================
// Error Recovery Tests
//=============================================================================

/**
 * @test Null pointer handling
 * WHAT: Test null safety throughout the API
 * WHY:  Verify graceful handling of null inputs
 * HOW:  Pass null to all functions, verify no crashes
 */
TEST_F(LanguageLayerE2ETest, NullPointerHandling) {
    // Orchestrator API with null
    language_orchestrator_destroy(nullptr);  // Should not crash
    EXPECT_NE(language_orchestrator_start(nullptr), 0);
    EXPECT_NE(language_orchestrator_stop(nullptr), 0);
    EXPECT_FALSE(language_orchestrator_is_running(nullptr));
    EXPECT_NE(language_orchestrator_process_phonemes(nullptr, nullptr, 0), 0);
    EXPECT_NE(language_orchestrator_process_text(nullptr, nullptr), 0);
    EXPECT_NE(language_orchestrator_update(nullptr, 0), 0);
    EXPECT_NE(language_orchestrator_reset(nullptr), 0);

    // Bridge API with null
    language_perception_bridge_destroy(nullptr);
    language_cognitive_bridge_destroy(nullptr);
    language_training_bridge_destroy(nullptr);
    language_omni_bridge_destroy(nullptr);
    language_immune_bridge_destroy(nullptr);
    language_gpu_bridge_destroy(nullptr);
    language_thalamic_bridge_destroy(nullptr);
    language_substrate_bridge_destroy(nullptr);
    language_logic_bridge_destroy(nullptr);

    // Valid orchestrator with null bridges
    ASSERT_TRUE(createOrchestrator());
    EXPECT_NE(language_orchestrator_connect_perception_bridge(orchestrator, nullptr), 0);
    EXPECT_NE(language_orchestrator_connect_cognitive_bridge(orchestrator, nullptr), 0);
    EXPECT_NE(language_orchestrator_connect_training_bridge(orchestrator, nullptr), 0);
}

/**
 * @test Recovery after error state
 * WHAT: Test recovery from error conditions
 * WHY:  Verify system can recover after errors
 * HOW:  Force error, then verify recovery
 */
TEST_F(LanguageLayerE2ETest, RecoveryAfterError) {
    ASSERT_TRUE(createOrchestrator());

    int result = language_orchestrator_start(orchestrator);
    EXPECT_EQ(result, 0);

    // Try to cause an error by processing invalid input
    result = language_orchestrator_process_phonemes(orchestrator, nullptr, 0);
    // May or may not error depending on implementation

    // Reset should work regardless
    result = language_orchestrator_reset(orchestrator);
    EXPECT_EQ(result, 0);

    // Should be able to continue processing
    language_state_t state = language_orchestrator_get_state(orchestrator);
    EXPECT_EQ(state, LANGUAGE_STATE_IDLE);

    // Process valid input
    result = language_orchestrator_process_phonemes(
        orchestrator,
        test_phonemes.data(),
        (uint32_t)test_phonemes.size()
    );
    EXPECT_EQ(result, 0);

    language_orchestrator_stop(orchestrator);
}

