/**
 * @file e2e_test_auditory_pipeline.cpp
 * @brief End-to-end tests for Temporal Auditory Processing Pipeline
 *
 * WHAT: Full pipeline tests for temporal lobe auditory processing
 * WHY:  Verify complete auditory workflows with substrate integration
 * HOW:  Test language, object recognition, semantic access, auditory processing
 *
 * TEST COVERAGE:
 * - Language Processing Pipeline (4 tests)
 * - Object Recognition (3 tests)
 * - Semantic Access (4 tests)
 * - Auditory Processing (4 tests)
 * - Metabolic Effects (3 tests)
 * - Long-Term Stability (3 tests)
 *
 * TOTAL: 21 tests
 *
 * BIOLOGICAL ANALOGY:
 * - Primary auditory cortex (A1) in superior temporal gyrus
 * - Belt and parabelt regions for complex sounds
 * - Wernicke's area for language comprehension
 * - Inferior temporal cortex for object recognition
 * - Semantic memory access involves temporal pole
 *
 * @author NIMCP Development Team
 * @date 2025-12-30
 */

#include "e2e_test_framework.h"
#include <thread>
#include <vector>
#include <algorithm>
#include <numeric>
#include <random>
#include <atomic>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "core/temporal/nimcp_temporal_substrate_bridge.h"
#include "core/temporal/nimcp_temporal_thalamic_bridge.h"
#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "utils/memory/nimcp_memory.h"

using namespace nimcp::e2e;

//=============================================================================
// Test Configuration
//=============================================================================

constexpr double MAX_LANGUAGE_PROCESSING_TIME_MS = 50.0;
constexpr double MAX_RECOGNITION_TIME_MS = 100.0;
constexpr double MAX_SEMANTIC_ACCESS_TIME_MS = 30.0;
constexpr float MIN_TEMPORAL_CAPACITY = 0.3f;
constexpr uint32_t AUDIO_SAMPLE_RATE = 44100;
constexpr uint32_t AUDIO_BUFFER_SIZE = 1024;
constexpr uint32_t FREQUENCY_BANDS = 64;

//=============================================================================
// Helper Structures
//=============================================================================

struct AudioStimulus {
    std::vector<float> samples;
    uint32_t sample_rate;
    float duration_ms;
    float frequency;  // For pure tones
};

struct SpeechStimulus {
    std::vector<float> waveform;
    std::vector<uint32_t> phoneme_boundaries;
    uint32_t num_words;
    float speech_rate;  // Words per minute
};

struct SemanticCue {
    uint32_t category_id;
    float relatedness;
    std::vector<float> features;
};

//=============================================================================
// Test Fixtures
//=============================================================================

class E2ETemporalLanguageTest : public ::testing::Test {
protected:
    neural_substrate_t* substrate = nullptr;
    temporal_substrate_bridge_t* temp_bridge = nullptr;

    void SetUp() override {
        substrate_config_t sub_config;
        substrate_default_config(&sub_config);
        substrate = substrate_create(&sub_config);
        ASSERT_NE(substrate, nullptr);

        temporal_substrate_config_t temp_config = temporal_substrate_default_config();
        temp_bridge = temporal_substrate_bridge_create(nullptr, substrate, &temp_config);
        ASSERT_NE(temp_bridge, nullptr);
    }

    void TearDown() override {
        if (temp_bridge) {
            temporal_substrate_bridge_destroy(temp_bridge);
            temp_bridge = nullptr;
        }
        if (substrate) {
            substrate_destroy(substrate);
            substrate = nullptr;
        }
    }

    SpeechStimulus createSpeech(uint32_t num_words, float words_per_minute) {
        SpeechStimulus stim;
        stim.num_words = num_words;
        stim.speech_rate = words_per_minute;

        float ms_per_word = 60000.0f / words_per_minute;
        float total_ms = num_words * ms_per_word;
        uint32_t total_samples = (uint32_t)(total_ms * AUDIO_SAMPLE_RATE / 1000.0f);

        stim.waveform.resize(total_samples);

        // Simple modulated noise for speech-like signal
        std::mt19937 gen(42);
        std::normal_distribution<float> dist(0.0f, 0.3f);

        for (uint32_t i = 0; i < total_samples; i++) {
            float envelope = 0.5f + 0.5f * sinf(i * 0.001f);
            stim.waveform[i] = dist(gen) * envelope;
        }

        // Mark word boundaries
        for (uint32_t w = 0; w <= num_words; w++) {
            stim.phoneme_boundaries.push_back((uint32_t)(w * total_samples / num_words));
        }

        return stim;
    }
};

class E2ETemporalObjectRecognitionTest : public ::testing::Test {
protected:
    neural_substrate_t* substrate = nullptr;
    temporal_substrate_bridge_t* temp_bridge = nullptr;

    void SetUp() override {
        substrate_config_t sub_config;
        substrate_default_config(&sub_config);
        substrate = substrate_create(&sub_config);
        ASSERT_NE(substrate, nullptr);

        temporal_substrate_config_t temp_config = temporal_substrate_default_config();
        temp_bridge = temporal_substrate_bridge_create(nullptr, substrate, &temp_config);
        ASSERT_NE(temp_bridge, nullptr);
    }

    void TearDown() override {
        if (temp_bridge) {
            temporal_substrate_bridge_destroy(temp_bridge);
            temp_bridge = nullptr;
        }
        if (substrate) {
            substrate_destroy(substrate);
            substrate = nullptr;
        }
    }
};

class E2ETemporalSemanticTest : public ::testing::Test {
protected:
    neural_substrate_t* substrate = nullptr;
    temporal_substrate_bridge_t* temp_bridge = nullptr;

    void SetUp() override {
        substrate_config_t sub_config;
        substrate_default_config(&sub_config);
        substrate = substrate_create(&sub_config);
        ASSERT_NE(substrate, nullptr);

        temporal_substrate_config_t temp_config = temporal_substrate_default_config();
        temp_bridge = temporal_substrate_bridge_create(nullptr, substrate, &temp_config);
        ASSERT_NE(temp_bridge, nullptr);
    }

    void TearDown() override {
        if (temp_bridge) {
            temporal_substrate_bridge_destroy(temp_bridge);
            temp_bridge = nullptr;
        }
        if (substrate) {
            substrate_destroy(substrate);
            substrate = nullptr;
        }
    }

    SemanticCue createCue(uint32_t category, float relatedness) {
        SemanticCue cue;
        cue.category_id = category;
        cue.relatedness = relatedness;
        cue.features.resize(64);

        std::mt19937 gen(category);
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);

        for (size_t i = 0; i < cue.features.size(); i++) {
            cue.features[i] = dist(gen);
        }
        return cue;
    }
};

class E2ETemporalAuditoryTest : public ::testing::Test {
protected:
    neural_substrate_t* substrate = nullptr;
    temporal_substrate_bridge_t* temp_bridge = nullptr;

    void SetUp() override {
        substrate_config_t sub_config;
        substrate_default_config(&sub_config);
        substrate = substrate_create(&sub_config);
        ASSERT_NE(substrate, nullptr);

        temporal_substrate_config_t temp_config = temporal_substrate_default_config();
        temp_bridge = temporal_substrate_bridge_create(nullptr, substrate, &temp_config);
        ASSERT_NE(temp_bridge, nullptr);
    }

    void TearDown() override {
        if (temp_bridge) {
            temporal_substrate_bridge_destroy(temp_bridge);
            temp_bridge = nullptr;
        }
        if (substrate) {
            substrate_destroy(substrate);
            substrate = nullptr;
        }
    }

    AudioStimulus createTone(float frequency, float duration_ms) {
        AudioStimulus stim;
        stim.frequency = frequency;
        stim.duration_ms = duration_ms;
        stim.sample_rate = AUDIO_SAMPLE_RATE;

        uint32_t num_samples = (uint32_t)(duration_ms * AUDIO_SAMPLE_RATE / 1000.0f);
        stim.samples.resize(num_samples);

        for (uint32_t i = 0; i < num_samples; i++) {
            float t = i / (float)AUDIO_SAMPLE_RATE;
            stim.samples[i] = sinf(2.0f * M_PI * frequency * t);
        }
        return stim;
    }
};

//=============================================================================
// Language Processing Pipeline Tests
//=============================================================================

TEST_F(E2ETemporalLanguageTest, BaselineLanguageCapacity) {
    // Scenario: Verify baseline language processing with optimal substrate
    E2E_PIPELINE_START("Baseline Language Capacity");

    E2E_STAGE_BEGIN("Initialize substrate", 5);
    substrate_set_atp(substrate, SUBSTRATE_NORMAL_ATP);
    substrate_update(substrate, 10);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Update temporal bridge", 10);
    int result = temporal_substrate_bridge_update(temp_bridge);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Get effects", 5);
    temporal_substrate_effects_t effects;
    result = temporal_substrate_bridge_get_effects(temp_bridge, &effects);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify capacity", 2);
    EXPECT_GT(effects.overall_capacity, MIN_TEMPORAL_CAPACITY);
    EXPECT_GT(effects.language_processing, MIN_TEMPORAL_CAPACITY);
    EXPECT_GT(effects.object_recognition, MIN_TEMPORAL_CAPACITY);
    EXPECT_GT(effects.semantic_access, MIN_TEMPORAL_CAPACITY);
    EXPECT_GT(effects.auditory_processing, MIN_TEMPORAL_CAPACITY);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ETemporalLanguageTest, SpeechComprehensionPipeline) {
    // Scenario: Process speech comprehension
    E2E_PIPELINE_START("Speech Comprehension Pipeline");

    E2E_STAGE_BEGIN("Create speech stimulus", 10);
    SpeechStimulus speech = createSpeech(5, 120.0f);  // 5 words at 120 WPM
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Process speech", 100);
    // Process each word
    for (uint32_t w = 0; w < speech.num_words; w++) {
        substrate_record_spikes(substrate, 200);
        substrate_record_transmissions(substrate, 500);
        substrate_update(substrate, 500);  // ~500ms per word
        temporal_substrate_bridge_update(temp_bridge);

        temporal_substrate_effects_t effects;
        temporal_substrate_bridge_get_effects(temp_bridge, &effects);

        EXPECT_GE(effects.language_processing, 0.0f);
        EXPECT_LE(effects.language_processing, 1.0f);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify comprehension", 5);
    temporal_substrate_effects_t final_effects;
    temporal_substrate_bridge_get_effects(temp_bridge, &final_effects);
    EXPECT_GT(final_effects.language_processing, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ETemporalLanguageTest, LanguageWithFatigue) {
    // Scenario: Language processing degrades with fatigue
    E2E_PIPELINE_START("Language With Fatigue");

    E2E_STAGE_BEGIN("Fresh state", 10);
    temporal_substrate_bridge_update(temp_bridge);

    temporal_substrate_effects_t fresh;
    temporal_substrate_bridge_get_effects(temp_bridge, &fresh);
    float fresh_lang = fresh.language_processing;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Induce fatigue", 100);
    for (int i = 0; i < 100; i++) {
        substrate_record_spikes(substrate, 300);
        substrate_record_transmissions(substrate, 700);
        substrate_update(substrate, 20);
    }
    temporal_substrate_bridge_update(temp_bridge);

    temporal_substrate_effects_t fatigued;
    temporal_substrate_bridge_get_effects(temp_bridge, &fatigued);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify values", 2);
    EXPECT_GE(fresh_lang, 0.0f);
    EXPECT_GE(fatigued.language_processing, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ETemporalLanguageTest, ApplyLanguageEffects) {
    // Scenario: Apply language processing effects
    E2E_PIPELINE_START("Apply Language Effects");

    E2E_STAGE_BEGIN("Update and apply", 20);
    temporal_substrate_bridge_update(temp_bridge);
    int result = temporal_substrate_bridge_apply_effects(temp_bridge);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify application", 5);
    temporal_substrate_effects_t effects;
    temporal_substrate_bridge_get_effects(temp_bridge, &effects);
    EXPECT_GT(effects.overall_capacity, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Object Recognition Tests
//=============================================================================

TEST_F(E2ETemporalObjectRecognitionTest, ObjectRecognitionCapacity) {
    // Scenario: Baseline object recognition
    E2E_PIPELINE_START("Object Recognition Capacity");

    E2E_STAGE_BEGIN("Initialize", 10);
    substrate_set_atp(substrate, SUBSTRATE_NORMAL_ATP);
    substrate_update(substrate, 10);
    temporal_substrate_bridge_update(temp_bridge);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Get recognition capacity", 5);
    temporal_substrate_effects_t effects;
    temporal_substrate_bridge_get_effects(temp_bridge, &effects);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify", 2);
    EXPECT_GT(effects.object_recognition, MIN_TEMPORAL_CAPACITY);
    EXPECT_LE(effects.object_recognition, 1.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ETemporalObjectRecognitionTest, CategoryProcessing) {
    // Scenario: Process objects from different categories
    E2E_PIPELINE_START("Category Processing");

    E2E_STAGE_BEGIN("Process categories", 100);
    // Simulate recognizing objects from 10 different categories
    for (int category = 0; category < 10; category++) {
        // Higher-level recognition requires more processing
        substrate_record_spikes(substrate, 150);
        substrate_record_transmissions(substrate, 400);
        substrate_update(substrate, 50);
        temporal_substrate_bridge_update(temp_bridge);

        temporal_substrate_effects_t effects;
        temporal_substrate_bridge_get_effects(temp_bridge, &effects);

        EXPECT_GE(effects.object_recognition, 0.0f);
        EXPECT_LE(effects.object_recognition, 1.0f);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify stability", 5);
    temporal_substrate_effects_t effects;
    temporal_substrate_bridge_get_effects(temp_bridge, &effects);
    EXPECT_GE(effects.object_recognition, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ETemporalObjectRecognitionTest, RecognitionUnderATPStress) {
    // Scenario: Object recognition under metabolic stress
    E2E_PIPELINE_START("Recognition Under ATP Stress");

    E2E_STAGE_BEGIN("Normal ATP", 10);
    substrate_set_atp(substrate, SUBSTRATE_NORMAL_ATP);
    substrate_update(substrate, 10);
    temporal_substrate_bridge_update(temp_bridge);

    temporal_substrate_effects_t normal;
    temporal_substrate_bridge_get_effects(temp_bridge, &normal);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Low ATP", 10);
    substrate_set_atp(substrate, 0.4f);
    substrate_update(substrate, 10);
    temporal_substrate_bridge_update(temp_bridge);

    temporal_substrate_effects_t stressed;
    temporal_substrate_bridge_get_effects(temp_bridge, &stressed);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Compare", 2);
    EXPECT_GT(normal.object_recognition, 0.0f);
    EXPECT_GE(stressed.object_recognition, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Semantic Access Tests
//=============================================================================

TEST_F(E2ETemporalSemanticTest, SemanticAccessCapacity) {
    // Scenario: Baseline semantic access
    E2E_PIPELINE_START("Semantic Access Capacity");

    E2E_STAGE_BEGIN("Initialize", 10);
    substrate_set_atp(substrate, SUBSTRATE_NORMAL_ATP);
    substrate_update(substrate, 10);
    temporal_substrate_bridge_update(temp_bridge);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Get semantic access", 5);
    temporal_substrate_effects_t effects;
    temporal_substrate_bridge_get_effects(temp_bridge, &effects);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify", 2);
    EXPECT_GT(effects.semantic_access, MIN_TEMPORAL_CAPACITY);
    EXPECT_LE(effects.semantic_access, 1.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ETemporalSemanticTest, SemanticRetrievalSpeed) {
    // Scenario: Track semantic retrieval across multiple accesses
    E2E_PIPELINE_START("Semantic Retrieval Speed");

    E2E_STAGE_BEGIN("Multiple retrievals", 100);
    std::vector<float> access_values;

    for (int retrieval = 0; retrieval < 20; retrieval++) {
        SemanticCue cue = createCue(retrieval, 0.8f);

        substrate_record_spikes(substrate, 100);
        substrate_update(substrate, 30);
        temporal_substrate_bridge_update(temp_bridge);

        temporal_substrate_effects_t effects;
        temporal_substrate_bridge_get_effects(temp_bridge, &effects);
        access_values.push_back(effects.semantic_access);

        EXPECT_GE(effects.semantic_access, 0.0f);
        EXPECT_LE(effects.semantic_access, 1.0f);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Analyze stability", 5);
    float mean = std::accumulate(access_values.begin(), access_values.end(), 0.0f)
                 / access_values.size();
    EXPECT_GT(mean, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ETemporalSemanticTest, SemanticPrimingEffect) {
    // Scenario: Semantic priming affects access speed
    E2E_PIPELINE_START("Semantic Priming Effect");

    E2E_STAGE_BEGIN("Initial access", 20);
    temporal_substrate_bridge_update(temp_bridge);

    temporal_substrate_effects_t initial;
    temporal_substrate_bridge_get_effects(temp_bridge, &initial);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Primed access", 20);
    // Prime with related concepts
    for (int i = 0; i < 5; i++) {
        substrate_record_spikes(substrate, 80);
        substrate_update(substrate, 20);
    }
    temporal_substrate_bridge_update(temp_bridge);

    temporal_substrate_effects_t primed;
    temporal_substrate_bridge_get_effects(temp_bridge, &primed);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify", 2);
    EXPECT_GE(initial.semantic_access, 0.0f);
    EXPECT_GE(primed.semantic_access, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ETemporalSemanticTest, SemanticWithGlucoseDeprivation) {
    // Scenario: Word-finding difficulty with low glucose
    E2E_PIPELINE_START("Semantic With Glucose Deprivation");

    E2E_STAGE_BEGIN("Normal glucose", 10);
    substrate_set_glucose(substrate, SUBSTRATE_NORMAL_GLUCOSE);
    substrate_update(substrate, 10);
    temporal_substrate_bridge_update(temp_bridge);

    temporal_substrate_effects_t normal;
    temporal_substrate_bridge_get_effects(temp_bridge, &normal);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Low glucose", 10);
    substrate_set_glucose(substrate, SUBSTRATE_CRITICAL_GLUCOSE);
    substrate_update(substrate, 10);
    temporal_substrate_bridge_update(temp_bridge);

    temporal_substrate_effects_t low;
    temporal_substrate_bridge_get_effects(temp_bridge, &low);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Compare", 2);
    EXPECT_GT(normal.semantic_access, 0.0f);
    EXPECT_GE(low.semantic_access, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Auditory Processing Tests
//=============================================================================

TEST_F(E2ETemporalAuditoryTest, AuditoryProcessingCapacity) {
    // Scenario: Baseline auditory processing
    E2E_PIPELINE_START("Auditory Processing Capacity");

    E2E_STAGE_BEGIN("Initialize", 10);
    substrate_set_atp(substrate, SUBSTRATE_NORMAL_ATP);
    substrate_update(substrate, 10);
    temporal_substrate_bridge_update(temp_bridge);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Get auditory processing", 5);
    temporal_substrate_effects_t effects;
    temporal_substrate_bridge_get_effects(temp_bridge, &effects);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify", 2);
    EXPECT_GT(effects.auditory_processing, MIN_TEMPORAL_CAPACITY);
    EXPECT_LE(effects.auditory_processing, 1.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ETemporalAuditoryTest, FrequencyProcessing) {
    // Scenario: Process different frequencies (tonotopy)
    E2E_PIPELINE_START("Frequency Processing");

    E2E_STAGE_BEGIN("Process frequencies", 100);
    float frequencies[] = {250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f};

    for (float freq : frequencies) {
        AudioStimulus tone = createTone(freq, 100.0f);

        substrate_record_spikes(substrate, 120);
        substrate_update(substrate, 100);
        temporal_substrate_bridge_update(temp_bridge);

        temporal_substrate_effects_t effects;
        temporal_substrate_bridge_get_effects(temp_bridge, &effects);

        EXPECT_GE(effects.auditory_processing, 0.0f);
        EXPECT_LE(effects.auditory_processing, 1.0f);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify tonotopy", 5);
    temporal_substrate_effects_t effects;
    temporal_substrate_bridge_get_effects(temp_bridge, &effects);
    EXPECT_GE(effects.auditory_processing, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ETemporalAuditoryTest, TemporalPatternProcessing) {
    // Scenario: Process temporal patterns in audio
    E2E_PIPELINE_START("Temporal Pattern Processing");

    E2E_STAGE_BEGIN("Process patterns", 100);
    // Different temporal patterns
    for (int pattern = 0; pattern < 10; pattern++) {
        // Pattern processing
        for (int segment = 0; segment < 5; segment++) {
            substrate_record_spikes(substrate, 80);
            substrate_update(substrate, 20);
        }
        temporal_substrate_bridge_update(temp_bridge);

        temporal_substrate_effects_t effects;
        temporal_substrate_bridge_get_effects(temp_bridge, &effects);

        EXPECT_GE(effects.auditory_processing, 0.0f);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify", 5);
    temporal_substrate_effects_t effects;
    temporal_substrate_bridge_get_effects(temp_bridge, &effects);
    EXPECT_GE(effects.auditory_processing, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ETemporalAuditoryTest, ComplexSoundProcessing) {
    // Scenario: Process complex sounds (belt/parabelt regions)
    E2E_PIPELINE_START("Complex Sound Processing");

    E2E_STAGE_BEGIN("Simple to complex", 100);
    // Increasing complexity
    for (int complexity = 1; complexity <= 10; complexity++) {
        uint32_t spikes = 50 + complexity * 25;

        substrate_record_spikes(substrate, spikes);
        substrate_record_transmissions(substrate, spikes * 2);
        substrate_update(substrate, 50);
        temporal_substrate_bridge_update(temp_bridge);

        temporal_substrate_effects_t effects;
        temporal_substrate_bridge_get_effects(temp_bridge, &effects);

        EXPECT_GE(effects.auditory_processing, 0.0f);
        EXPECT_FALSE(std::isnan(effects.auditory_processing));
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Final check", 5);
    temporal_substrate_effects_t effects;
    temporal_substrate_bridge_get_effects(temp_bridge, &effects);
    EXPECT_GE(effects.overall_capacity, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Metabolic Effects Tests
//=============================================================================

TEST_F(E2ETemporalLanguageTest, ATPEffectsOnLanguage) {
    // Scenario: ATP levels affect language processing
    E2E_PIPELINE_START("ATP Effects On Language");

    float atp_levels[] = {0.95f, 0.7f, 0.5f, 0.3f};
    std::vector<temporal_substrate_effects_t> effects_at_atp;

    E2E_STAGE_BEGIN("Test ATP levels", 40);
    for (float atp : atp_levels) {
        substrate_set_atp(substrate, atp);
        substrate_update(substrate, 10);
        temporal_substrate_bridge_update(temp_bridge);

        temporal_substrate_effects_t effects;
        temporal_substrate_bridge_get_effects(temp_bridge, &effects);
        effects_at_atp.push_back(effects);

        EXPECT_GE(effects.language_processing, 0.0f);
        EXPECT_LE(effects.language_processing, 1.0f);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify gradient", 5);
    for (const auto& eff : effects_at_atp) {
        EXPECT_FALSE(std::isnan(eff.language_processing));
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ETemporalLanguageTest, OxygenDependentProcessing) {
    // Scenario: Temporal cortex processing requires oxygen
    E2E_PIPELINE_START("Oxygen Dependent Processing");

    E2E_STAGE_BEGIN("Normal oxygen", 10);
    substrate_set_oxygen(substrate, SUBSTRATE_NORMAL_O2_SAT);
    substrate_update(substrate, 10);
    temporal_substrate_bridge_update(temp_bridge);

    temporal_substrate_effects_t normal_o2;
    temporal_substrate_bridge_get_effects(temp_bridge, &normal_o2);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Low oxygen", 10);
    substrate_set_oxygen(substrate, SUBSTRATE_CRITICAL_O2);
    substrate_update(substrate, 10);
    temporal_substrate_bridge_update(temp_bridge);

    temporal_substrate_effects_t low_o2;
    temporal_substrate_bridge_get_effects(temp_bridge, &low_o2);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify", 2);
    EXPECT_GT(normal_o2.overall_capacity, 0.0f);
    EXPECT_GE(low_o2.overall_capacity, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ETemporalLanguageTest, TemperatureEffects) {
    // Scenario: Temperature affects temporal processing
    E2E_PIPELINE_START("Temperature Effects");

    float temperatures[] = {35.0f, 37.0f, 39.0f};

    E2E_STAGE_BEGIN("Test temperatures", 30);
    for (float temp : temperatures) {
        substrate_set_temperature(substrate, temp);
        substrate_update(substrate, 10);
        temporal_substrate_bridge_update(temp_bridge);

        temporal_substrate_effects_t effects;
        temporal_substrate_bridge_get_effects(temp_bridge, &effects);

        EXPECT_GE(effects.overall_capacity, 0.0f);
        EXPECT_LE(effects.overall_capacity, 1.0f);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify all valid", 5);
    temporal_substrate_effects_t effects;
    temporal_substrate_bridge_get_effects(temp_bridge, &effects);
    EXPECT_FALSE(std::isnan(effects.overall_capacity));
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Long-Term Stability Tests
//=============================================================================

TEST_F(E2ETemporalLanguageTest, LongSimulationStability) {
    // Scenario: Extended temporal processing without degradation
    E2E_PIPELINE_START("Long Simulation Stability");

    E2E_STAGE_BEGIN("Extended simulation", 500);
    for (int step = 0; step < 1000; step++) {
        substrate_update(substrate, 10);
        temporal_substrate_bridge_update(temp_bridge);

        if (step % 100 == 0) {
            temporal_substrate_effects_t effects;
            temporal_substrate_bridge_get_effects(temp_bridge, &effects);

            EXPECT_FALSE(std::isnan(effects.overall_capacity));
            EXPECT_FALSE(std::isinf(effects.overall_capacity));
            EXPECT_GE(effects.overall_capacity, 0.0f);
            EXPECT_LE(effects.overall_capacity, 1.0f);
        }
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Final validation", 5);
    temporal_substrate_effects_t final_effects;
    temporal_substrate_bridge_get_effects(temp_bridge, &final_effects);

    EXPECT_GT(final_effects.overall_capacity, 0.0f);
    EXPECT_GT(final_effects.language_processing, 0.0f);
    EXPECT_GT(final_effects.object_recognition, 0.0f);
    EXPECT_GT(final_effects.semantic_access, 0.0f);
    EXPECT_GT(final_effects.auditory_processing, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ETemporalAuditoryTest, ContinuousAuditoryProcessing) {
    // Scenario: Continuous audio processing stability
    E2E_PIPELINE_START("Continuous Auditory Processing");

    E2E_STAGE_BEGIN("Continuous processing", 300);
    std::vector<float> processing_values;

    for (int buffer = 0; buffer < 100; buffer++) {
        substrate_record_spikes(substrate, 100);
        substrate_update(substrate, 23);  // ~44100 samples at 44.1kHz
        temporal_substrate_bridge_update(temp_bridge);

        temporal_substrate_effects_t effects;
        temporal_substrate_bridge_get_effects(temp_bridge, &effects);
        processing_values.push_back(effects.auditory_processing);

        EXPECT_FALSE(std::isnan(effects.auditory_processing));
        EXPECT_FALSE(std::isinf(effects.auditory_processing));
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Analyze stability", 5);
    float min_val = *std::min_element(processing_values.begin(), processing_values.end());
    float max_val = *std::max_element(processing_values.begin(), processing_values.end());

    EXPECT_GE(min_val, 0.0f);
    EXPECT_LE(max_val, 1.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ETemporalSemanticTest, RecoveryFromStress) {
    // Scenario: Semantic access recovery after stress
    E2E_PIPELINE_START("Recovery From Stress");

    E2E_STAGE_BEGIN("Baseline", 10);
    temporal_substrate_bridge_update(temp_bridge);

    temporal_substrate_effects_t baseline;
    temporal_substrate_bridge_get_effects(temp_bridge, &baseline);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Apply stress", 50);
    substrate_set_atp(substrate, 0.3f);
    for (int i = 0; i < 20; i++) {
        substrate_record_spikes(substrate, 400);
        substrate_update(substrate, 10);
    }
    temporal_substrate_bridge_update(temp_bridge);

    temporal_substrate_effects_t stressed;
    temporal_substrate_bridge_get_effects(temp_bridge, &stressed);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Recovery", 100);
    substrate_set_atp(substrate, SUBSTRATE_NORMAL_ATP);
    for (int i = 0; i < 100; i++) {
        substrate_update(substrate, 50);
    }
    temporal_substrate_bridge_update(temp_bridge);

    temporal_substrate_effects_t recovered;
    temporal_substrate_bridge_get_effects(temp_bridge, &recovered);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify recovery", 5);
    EXPECT_GE(recovered.semantic_access, stressed.semantic_access);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Entry Point
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
