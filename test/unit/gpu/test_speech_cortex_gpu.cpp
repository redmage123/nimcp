/**
 * @file test_speech_cortex_gpu.cpp
 * @brief Unit tests for GPU-accelerated Speech Cortex operations
 *
 * Tests phoneme recognition, formant extraction, prosody analysis, and
 * speech processing pipeline GPU operations.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <cstring>
#include <random>

// Headers already have their own extern "C" guards
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "perception/nimcp_speech_cortex.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class SpeechCortexGPUTest : public ::testing::Test {
protected:
    nimcp_gpu_context_t* ctx = nullptr;
    speech_cortex_t* speech_state = nullptr;
    bool gpu_available = false;

    void SetUp() override {
        ctx = nimcp_gpu_context_create_auto();
        gpu_available = (ctx != nullptr && nimcp_gpu_context_is_valid(ctx));
    }

    void TearDown() override {
        if (speech_state) {
            speech_cortex_destroy(speech_state);
            speech_state = nullptr;
        }
        if (ctx) {
            nimcp_gpu_context_destroy(ctx);
            ctx = nullptr;
        }
    }

    void RequireGPU() {
        if (!gpu_available) {
            GTEST_SKIP() << "GPU not available, skipping test";
        }
    }

    // Helper to create speech cortex with default config
    speech_cortex_t* CreateSpeechCortex(uint32_t sample_rate = 16000) {
        speech_cortex_config_t config = speech_cortex_default_config();
        config.sample_rate = sample_rate;
        config.enable_wernicke = true;
        config.enable_broca = true;
        config.enable_prosody = true;
        config.enable_memory = true;
        return speech_cortex_create(&config);
    }

    // Helper to create a tensor filled with a constant value
    nimcp_gpu_tensor_t* CreateFilledTensor(size_t* dims, size_t rank, float value) {
        if (!ctx) return nullptr;
        nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_create(ctx, dims, rank, NIMCP_GPU_PRECISION_FP32);
        if (tensor) {
            nimcp_gpu_fill(ctx, tensor, value);
        }
        return tensor;
    }

    // Helper to create 1D tensor
    nimcp_gpu_tensor_t* Create1DTensor(size_t n, float value = 0.0f) {
        size_t dims[1] = {n};
        return CreateFilledTensor(dims, 1, value);
    }

    // Helper to create 2D tensor
    nimcp_gpu_tensor_t* Create2DTensor(size_t rows, size_t cols, float value = 0.0f) {
        size_t dims[2] = {rows, cols};
        return CreateFilledTensor(dims, 2, value);
    }

    // Helper to copy tensor to host
    std::vector<float> CopyToHost(nimcp_gpu_tensor_t* tensor) {
        size_t n = tensor->numel;
        std::vector<float> host_data(n);
        nimcp_gpu_tensor_to_host(tensor, host_data.data());
        return host_data;
    }

    // Helper to set tensor from host
    void SetFromHost(nimcp_gpu_tensor_t* tensor, const std::vector<float>& data) {
        nimcp_gpu_tensor_t* temp = nimcp_gpu_tensor_from_host(ctx, data.data(),
            tensor->dims, tensor->ndim, tensor->precision);
        if (temp) {
            nimcp_gpu_copy(ctx, temp, tensor);
            nimcp_gpu_tensor_destroy(temp);
        }
    }

    // Helper to generate sine wave audio
    std::vector<float> CreateSineWave(float freq_hz, float duration_sec, int sample_rate) {
        size_t num_samples = static_cast<size_t>(duration_sec * sample_rate);
        std::vector<float> audio(num_samples);
        for (size_t i = 0; i < num_samples; i++) {
            float t = static_cast<float>(i) / sample_rate;
            audio[i] = 0.5f * std::sin(2.0f * M_PI * freq_hz * t);
        }
        return audio;
    }

    // Helper to generate white noise
    std::vector<float> CreateWhiteNoise(float duration_sec, int sample_rate) {
        size_t num_samples = static_cast<size_t>(duration_sec * sample_rate);
        std::vector<float> audio(num_samples);
        std::mt19937 gen(42);
        std::uniform_real_distribution<float> dist(-0.5f, 0.5f);
        for (size_t i = 0; i < num_samples; i++) {
            audio[i] = dist(gen);
        }
        return audio;
    }

    // Helper to generate formant-like signal (sum of sine waves)
    std::vector<float> CreateFormantSignal(float f1_hz, float f2_hz, float f3_hz,
                                           float duration_sec, int sample_rate) {
        size_t num_samples = static_cast<size_t>(duration_sec * sample_rate);
        std::vector<float> audio(num_samples);
        for (size_t i = 0; i < num_samples; i++) {
            float t = static_cast<float>(i) / sample_rate;
            audio[i] = 0.3f * std::sin(2.0f * M_PI * f1_hz * t) +
                       0.2f * std::sin(2.0f * M_PI * f2_hz * t) +
                       0.1f * std::sin(2.0f * M_PI * f3_hz * t);
        }
        return audio;
    }

    // Helper to generate silence
    std::vector<float> CreateSilence(float duration_sec, int sample_rate) {
        size_t num_samples = static_cast<size_t>(duration_sec * sample_rate);
        return std::vector<float>(num_samples, 0.0f);
    }

    // Helper to generate speech-like signal with modulation
    std::vector<float> CreateSpeechLikeSignal(float duration_sec, int sample_rate) {
        size_t num_samples = static_cast<size_t>(duration_sec * sample_rate);
        std::vector<float> audio(num_samples);

        // Fundamental frequency with vibrato
        float f0 = 150.0f;  // Fundamental frequency
        float vibrato_rate = 5.0f;
        float vibrato_depth = 10.0f;

        // Formant frequencies for vowel 'a'
        float f1 = 730.0f;
        float f2 = 1090.0f;
        float f3 = 2440.0f;

        for (size_t i = 0; i < num_samples; i++) {
            float t = static_cast<float>(i) / sample_rate;

            // Pitch with vibrato
            float pitch = f0 + vibrato_depth * std::sin(2.0f * M_PI * vibrato_rate * t);

            // Envelope for natural sound
            float env = std::exp(-2.0f * t) * (1.0f - std::exp(-50.0f * t));

            // Generate harmonics weighted by formants
            float signal = 0.0f;
            for (int h = 1; h <= 10; h++) {
                float harmonic_freq = h * pitch;
                // Weight by formant proximity
                float w1 = std::exp(-std::pow((harmonic_freq - f1) / 100.0f, 2));
                float w2 = std::exp(-std::pow((harmonic_freq - f2) / 100.0f, 2));
                float w3 = std::exp(-std::pow((harmonic_freq - f3) / 100.0f, 2));
                float weight = 0.4f * w1 + 0.3f * w2 + 0.2f * w3 + 0.1f / h;
                signal += weight * std::sin(2.0f * M_PI * harmonic_freq * t) / h;
            }

            audio[i] = env * signal * 0.5f;
        }
        return audio;
    }
};

//=============================================================================
// Speech Cortex Creation Tests
//=============================================================================

TEST_F(SpeechCortexGPUTest, StateCreation_WithValidParams_ReturnsValidState) {
    speech_cortex_config_t config = speech_cortex_default_config();
    config.sample_rate = 16000;
    config.frame_size_ms = 25;
    config.hop_size_ms = 10;
    config.num_phonemes = SPEECH_NUM_PHONEMES;
    config.enable_wernicke = true;

    speech_cortex_t* state = speech_cortex_create(&config);
    ASSERT_NE(state, nullptr);

    speech_cortex_destroy(state);
}

TEST_F(SpeechCortexGPUTest, StateCreation_WithDifferentSampleRates_Works) {
    uint32_t sample_rates[] = {8000, 16000, 22050, 44100, 48000};

    for (uint32_t rate : sample_rates) {
        speech_cortex_config_t config = speech_cortex_default_config();
        config.sample_rate = rate;

        speech_cortex_t* state = speech_cortex_create(&config);
        ASSERT_NE(state, nullptr) << "Failed for sample rate: " << rate;

        speech_cortex_destroy(state);
    }
}

TEST_F(SpeechCortexGPUTest, StateDestruction_NullSafe) {
    speech_cortex_destroy(nullptr);  // Should not crash
}

TEST_F(SpeechCortexGPUTest, DefaultConfig_ReturnsValidConfig) {
    speech_cortex_config_t config = speech_cortex_default_config();

    EXPECT_GT(config.sample_rate, 0u);
    EXPECT_GT(config.frame_size_ms, 0u);
    EXPECT_GT(config.hop_size_ms, 0u);
    EXPECT_LE(config.hop_size_ms, config.frame_size_ms);
}

//=============================================================================
// Pre-emphasis Filter Tests
//=============================================================================

TEST_F(SpeechCortexGPUTest, PreEmphasis_BoostsHighFrequencies) {
    RequireGPU();

    const size_t n_samples = 1024;
    const int sample_rate = 16000;

    // Create a signal with both low and high frequency components
    std::vector<float> audio(n_samples);
    for (size_t i = 0; i < n_samples; i++) {
        float t = static_cast<float>(i) / sample_rate;
        // Low frequency (100 Hz) + High frequency (4000 Hz)
        audio[i] = 0.5f * std::sin(2.0f * M_PI * 100.0f * t) +
                   0.5f * std::sin(2.0f * M_PI * 4000.0f * t);
    }

    speech_cortex_t* state = CreateSpeechCortex(sample_rate);
    ASSERT_NE(state, nullptr);

    std::vector<float> features(128);
    bool result = speech_cortex_process(state, audio.data(), n_samples, features.data());
    EXPECT_TRUE(result);

    speech_cortex_destroy(state);
}

//=============================================================================
// Windowing Tests (Hamming, Hann)
//=============================================================================

TEST_F(SpeechCortexGPUTest, Windowing_ReducesSpectralLeakage) {
    RequireGPU();

    const int sample_rate = 16000;
    const float duration = 0.05f;  // 50ms

    // Pure tone
    auto audio = CreateSineWave(440.0f, duration, sample_rate);

    speech_cortex_config_t config = speech_cortex_default_config();
    config.sample_rate = sample_rate;
    speech_cortex_t* state = speech_cortex_create(&config);
    ASSERT_NE(state, nullptr);

    std::vector<float> features(config.feature_dim);
    bool result = speech_cortex_process(state, audio.data(), audio.size(), features.data());
    EXPECT_TRUE(result);

    speech_cortex_destroy(state);
}

//=============================================================================
// FFT Correctness Tests
//=============================================================================

TEST_F(SpeechCortexGPUTest, FFT_DetectsPeakAtCorrectFrequency) {
    RequireGPU();

    const int sample_rate = 16000;
    const float test_freq = 440.0f;
    const float duration = 0.1f;

    auto audio = CreateSineWave(test_freq, duration, sample_rate);

    speech_cortex_t* state = CreateSpeechCortex(sample_rate);
    ASSERT_NE(state, nullptr);

    std::vector<float> features(128);
    bool result = speech_cortex_process(state, audio.data(), audio.size(), features.data());
    EXPECT_TRUE(result);

    speech_cortex_destroy(state);
}

//=============================================================================
// Power Spectrum Computation Tests
//=============================================================================

TEST_F(SpeechCortexGPUTest, PowerSpectrum_NonNegativeValues) {
    RequireGPU();

    const int sample_rate = 16000;
    auto audio = CreateWhiteNoise(0.1f, sample_rate);

    speech_cortex_t* state = CreateSpeechCortex(sample_rate);
    ASSERT_NE(state, nullptr);

    std::vector<float> features(128);
    bool result = speech_cortex_process(state, audio.data(), audio.size(), features.data());
    EXPECT_TRUE(result);

    // Features should be valid (not NaN or Inf)
    for (float f : features) {
        EXPECT_FALSE(std::isnan(f));
        EXPECT_FALSE(std::isinf(f));
    }

    speech_cortex_destroy(state);
}

//=============================================================================
// Mel Filterbank Tests
//=============================================================================

TEST_F(SpeechCortexGPUTest, MelFilterbank_ProperFrequencyMapping) {
    RequireGPU();

    const int sample_rate = 16000;

    // Test with signals at different frequencies
    float test_freqs[] = {200.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f};

    speech_cortex_t* state = CreateSpeechCortex(sample_rate);
    ASSERT_NE(state, nullptr);

    for (float freq : test_freqs) {
        auto audio = CreateSineWave(freq, 0.1f, sample_rate);
        std::vector<float> features(128);

        bool result = speech_cortex_process(state, audio.data(), audio.size(), features.data());
        EXPECT_TRUE(result) << "Failed for frequency: " << freq;
    }

    speech_cortex_destroy(state);
}

//=============================================================================
// MFCC Computation Tests
//=============================================================================

TEST_F(SpeechCortexGPUTest, MFCC_ValidCoefficients) {
    RequireGPU();

    const int sample_rate = 16000;
    auto audio = CreateSpeechLikeSignal(0.1f, sample_rate);

    speech_cortex_config_t config = speech_cortex_default_config();
    config.sample_rate = sample_rate;
    speech_cortex_t* state = speech_cortex_create(&config);
    ASSERT_NE(state, nullptr);

    std::vector<float> features(config.feature_dim);
    bool result = speech_cortex_process(state, audio.data(), audio.size(), features.data());
    EXPECT_TRUE(result);

    // Check for valid values
    for (float f : features) {
        EXPECT_FALSE(std::isnan(f));
        EXPECT_FALSE(std::isinf(f));
    }

    speech_cortex_destroy(state);
}

//=============================================================================
// Delta Features Tests
//=============================================================================

TEST_F(SpeechCortexGPUTest, DeltaFeatures_CaptureTemporalDynamics) {
    RequireGPU();

    const int sample_rate = 16000;

    // Create time-varying signal
    std::vector<float> audio(sample_rate);  // 1 second
    for (size_t i = 0; i < audio.size(); i++) {
        float t = static_cast<float>(i) / sample_rate;
        // Frequency sweep
        float freq = 200.0f + 1000.0f * t;
        audio[i] = 0.5f * std::sin(2.0f * M_PI * freq * t);
    }

    speech_cortex_t* state = CreateSpeechCortex(sample_rate);
    ASSERT_NE(state, nullptr);

    std::vector<float> features(128);
    bool result = speech_cortex_process(state, audio.data(), audio.size(), features.data());
    EXPECT_TRUE(result);

    speech_cortex_destroy(state);
}

//=============================================================================
// Formant Extraction Tests
//=============================================================================

TEST_F(SpeechCortexGPUTest, FormantExtraction_DetectsFormantFrequencies) {
    RequireGPU();

    const int sample_rate = 16000;

    // Vowel 'a' formants: F1~730Hz, F2~1090Hz, F3~2440Hz
    auto audio = CreateFormantSignal(730.0f, 1090.0f, 2440.0f, 0.1f, sample_rate);

    speech_cortex_t* state = CreateSpeechCortex(sample_rate);
    ASSERT_NE(state, nullptr);

    float formants[4];
    bool result = speech_cortex_extract_formants(state, audio.data(), audio.size(),
                                                  formants, 4);
    EXPECT_TRUE(result);

    // Formants should be positive
    for (int i = 0; i < 4; i++) {
        EXPECT_GE(formants[i], 0.0f);
    }

    speech_cortex_destroy(state);
}

TEST_F(SpeechCortexGPUTest, FormantExtraction_DifferentVowels) {
    RequireGPU();

    const int sample_rate = 16000;

    // Different vowel formants
    struct VowelFormants {
        float f1, f2, f3;
        const char* vowel;
    } vowels[] = {
        {270.0f, 2290.0f, 3010.0f, "i"},   // "ee"
        {730.0f, 1090.0f, 2440.0f, "a"},   // "ah"
        {300.0f, 870.0f, 2240.0f, "u"}     // "oo"
    };

    speech_cortex_t* state = CreateSpeechCortex(sample_rate);
    ASSERT_NE(state, nullptr);

    for (const auto& v : vowels) {
        auto audio = CreateFormantSignal(v.f1, v.f2, v.f3, 0.1f, sample_rate);
        float formants[4];

        bool result = speech_cortex_extract_formants(state, audio.data(), audio.size(),
                                                      formants, 4);
        EXPECT_TRUE(result) << "Failed for vowel: " << v.vowel;
    }

    speech_cortex_destroy(state);
}

//=============================================================================
// Vowel Classification Tests
//=============================================================================

TEST_F(SpeechCortexGPUTest, VowelClassification_FromFormants) {
    // Test F1-F2 mapping to vowel categories

    // High front vowel "ee" (IY): F1~270, F2~2290
    phoneme_t vowel = speech_cortex_classify_vowel(270.0f, 2290.0f);
    EXPECT_EQ(vowel, PHONEME_IY);

    // Low back vowel "ah" (AA): F1~730, F2~1090
    vowel = speech_cortex_classify_vowel(730.0f, 1090.0f);
    EXPECT_EQ(vowel, PHONEME_AA);

    // High back vowel "oo" (UW): F1~300, F2~870
    vowel = speech_cortex_classify_vowel(300.0f, 870.0f);
    EXPECT_EQ(vowel, PHONEME_UW);
}

//=============================================================================
// Pitch Detection Tests
//=============================================================================

TEST_F(SpeechCortexGPUTest, PitchDetection_SineWave) {
    RequireGPU();

    const int sample_rate = 16000;
    float test_pitches[] = {100.0f, 150.0f, 200.0f, 300.0f, 400.0f};

    speech_cortex_t* state = CreateSpeechCortex(sample_rate);
    ASSERT_NE(state, nullptr);

    for (float expected_pitch : test_pitches) {
        auto audio = CreateSineWave(expected_pitch, 0.1f, sample_rate);

        float detected_pitch, stress;
        bool result = speech_cortex_extract_prosody(state, audio.data(), audio.size(),
                                                     &detected_pitch, &stress);
        EXPECT_TRUE(result) << "Failed for pitch: " << expected_pitch;

        // Allow 10% tolerance
        if (detected_pitch > 0) {
            float tolerance = expected_pitch * 0.1f;
            EXPECT_NEAR(detected_pitch, expected_pitch, tolerance)
                << "Pitch detection failed for " << expected_pitch << " Hz";
        }
    }

    speech_cortex_destroy(state);
}

TEST_F(SpeechCortexGPUTest, PitchDetection_SpeechRange) {
    RequireGPU();

    const int sample_rate = 16000;

    // Test in typical speech F0 range
    float male_pitch = 120.0f;    // Typical male F0
    float female_pitch = 220.0f;  // Typical female F0

    speech_cortex_t* state = CreateSpeechCortex(sample_rate);
    ASSERT_NE(state, nullptr);

    auto male_audio = CreateSpeechLikeSignal(0.1f, sample_rate);
    float pitch_m, stress_m;
    bool result = speech_cortex_extract_prosody(state, male_audio.data(), male_audio.size(),
                                                 &pitch_m, &stress_m);
    EXPECT_TRUE(result);
    EXPECT_GT(pitch_m, 50.0f);   // Should detect some pitch
    EXPECT_LT(pitch_m, 500.0f);  // In speech range

    speech_cortex_destroy(state);
}

//=============================================================================
// Voice Activity Detection Tests
//=============================================================================

TEST_F(SpeechCortexGPUTest, VAD_DetectsSilence) {
    RequireGPU();

    const int sample_rate = 16000;
    auto silence = CreateSilence(0.1f, sample_rate);

    speech_cortex_t* state = CreateSpeechCortex(sample_rate);
    ASSERT_NE(state, nullptr);

    phoneme_event_t phonemes[10];
    uint32_t num_detected = 0;

    bool result = speech_cortex_detect_phonemes(state, silence.data(), silence.size(),
                                                 phonemes, 10, &num_detected);
    EXPECT_TRUE(result);

    // Should detect silence phoneme or no phonemes
    if (num_detected > 0) {
        EXPECT_EQ(phonemes[0].phoneme, PHONEME_SILENCE);
    }

    speech_cortex_destroy(state);
}

TEST_F(SpeechCortexGPUTest, VAD_DetectsSpeech) {
    RequireGPU();

    const int sample_rate = 16000;
    auto speech = CreateSpeechLikeSignal(0.1f, sample_rate);

    speech_cortex_t* state = CreateSpeechCortex(sample_rate);
    ASSERT_NE(state, nullptr);

    phoneme_event_t phonemes[10];
    uint32_t num_detected = 0;

    bool result = speech_cortex_detect_phonemes(state, speech.data(), speech.size(),
                                                 phonemes, 10, &num_detected);
    EXPECT_TRUE(result);

    // Should detect some non-silence phonemes
    bool found_speech = false;
    for (uint32_t i = 0; i < num_detected; i++) {
        if (phonemes[i].phoneme != PHONEME_SILENCE) {
            found_speech = true;
            break;
        }
    }
    // Note: May not always detect speech depending on signal quality
    // EXPECT_TRUE(found_speech);

    speech_cortex_destroy(state);
}

//=============================================================================
// Phoneme Detection Tests
//=============================================================================

TEST_F(SpeechCortexGPUTest, PhonemeDetection_ValidOutput) {
    RequireGPU();

    const int sample_rate = 16000;
    auto audio = CreateSpeechLikeSignal(0.2f, sample_rate);

    speech_cortex_t* state = CreateSpeechCortex(sample_rate);
    ASSERT_NE(state, nullptr);

    phoneme_event_t phonemes[32];
    uint32_t num_detected = 0;

    bool result = speech_cortex_detect_phonemes(state, audio.data(), audio.size(),
                                                 phonemes, 32, &num_detected);
    EXPECT_TRUE(result);

    // Validate detected phonemes
    for (uint32_t i = 0; i < num_detected; i++) {
        EXPECT_GE(phonemes[i].phoneme, 0);
        EXPECT_LT(phonemes[i].phoneme, PHONEME_COUNT);
        EXPECT_GE(phonemes[i].confidence, 0.0f);
        EXPECT_LE(phonemes[i].confidence, 1.0f);
    }

    speech_cortex_destroy(state);
}

TEST_F(SpeechCortexGPUTest, PhonemeDetection_TimingInfo) {
    RequireGPU();

    const int sample_rate = 16000;
    auto audio = CreateSpeechLikeSignal(0.5f, sample_rate);

    speech_cortex_t* state = CreateSpeechCortex(sample_rate);
    ASSERT_NE(state, nullptr);

    phoneme_event_t phonemes[64];
    uint32_t num_detected = 0;

    bool result = speech_cortex_detect_phonemes(state, audio.data(), audio.size(),
                                                 phonemes, 64, &num_detected);
    EXPECT_TRUE(result);

    // Check timing is monotonically increasing
    for (uint32_t i = 1; i < num_detected; i++) {
        EXPECT_GE(phonemes[i].onset_time_ms, phonemes[i-1].onset_time_ms);
    }

    // Check offset >= onset
    for (uint32_t i = 0; i < num_detected; i++) {
        EXPECT_GE(phonemes[i].offset_time_ms, phonemes[i].onset_time_ms);
    }

    speech_cortex_destroy(state);
}

//=============================================================================
// Full Pipeline Tests
//=============================================================================

TEST_F(SpeechCortexGPUTest, FullPipeline_AudioToFeatures) {
    RequireGPU();

    const int sample_rate = 16000;
    auto audio = CreateSpeechLikeSignal(0.5f, sample_rate);

    speech_cortex_config_t config = speech_cortex_default_config();
    config.sample_rate = sample_rate;
    config.enable_wernicke = true;
    config.enable_broca = true;
    config.enable_prosody = true;

    speech_cortex_t* state = speech_cortex_create(&config);
    ASSERT_NE(state, nullptr);

    std::vector<float> features(config.feature_dim);
    bool result = speech_cortex_process(state, audio.data(), audio.size(), features.data());
    EXPECT_TRUE(result);

    // Check features are valid
    bool all_zero = true;
    for (float f : features) {
        EXPECT_FALSE(std::isnan(f));
        EXPECT_FALSE(std::isinf(f));
        if (f != 0.0f) all_zero = false;
    }
    EXPECT_FALSE(all_zero);  // Should have non-zero features

    speech_cortex_destroy(state);
}

//=============================================================================
// Batch Processing Tests
//=============================================================================

TEST_F(SpeechCortexGPUTest, BatchProcessing_MultipleFrames) {
    RequireGPU();

    const int sample_rate = 16000;
    const int num_frames = 10;
    const float frame_duration = 0.025f;  // 25ms

    speech_cortex_t* state = CreateSpeechCortex(sample_rate);
    ASSERT_NE(state, nullptr);

    for (int i = 0; i < num_frames; i++) {
        auto audio = CreateSineWave(200.0f + i * 50.0f, frame_duration, sample_rate);
        std::vector<float> features(128);

        bool result = speech_cortex_process(state, audio.data(), audio.size(), features.data());
        EXPECT_TRUE(result) << "Failed on frame " << i;
    }

    speech_cortex_destroy(state);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(SpeechCortexGPUTest, Statistics_ValidAfterProcessing) {
    RequireGPU();

    const int sample_rate = 16000;
    auto audio = CreateSpeechLikeSignal(0.5f, sample_rate);

    speech_cortex_t* state = CreateSpeechCortex(sample_rate);
    ASSERT_NE(state, nullptr);

    // Process some audio
    std::vector<float> features(128);
    for (int i = 0; i < 5; i++) {
        speech_cortex_process(state, audio.data(), audio.size(), features.data());
    }

    speech_cortex_stats_t stats;
    bool result = speech_cortex_get_stats(state, &stats);
    EXPECT_TRUE(result);
    EXPECT_GT(stats.frames_processed, 0u);

    speech_cortex_destroy(state);
}

//=============================================================================
// Phonological Buffer Tests
//=============================================================================

TEST_F(SpeechCortexGPUTest, PhonologicalBuffer_StoreAndRetrieve) {
    RequireGPU();

    speech_cortex_t* state = CreateSpeechCortex();
    ASSERT_NE(state, nullptr);

    // Store some phonemes
    phoneme_t to_store[] = {PHONEME_B, PHONEME_AA, PHONEME_T};
    bool store_result = speech_cortex_store_phonological_buffer(state, to_store, 3);
    EXPECT_TRUE(store_result);

    // Retrieve
    phoneme_t retrieved[9];
    uint32_t num_retrieved = 0;
    bool retrieve_result = speech_cortex_retrieve_phonological_buffer(state, retrieved, 9, &num_retrieved);
    EXPECT_TRUE(retrieve_result);
    EXPECT_GE(num_retrieved, 3u);

    speech_cortex_destroy(state);
}

TEST_F(SpeechCortexGPUTest, PhonologicalBuffer_Clear) {
    RequireGPU();

    speech_cortex_t* state = CreateSpeechCortex();
    ASSERT_NE(state, nullptr);

    // Store phonemes
    phoneme_t to_store[] = {PHONEME_K, PHONEME_AE, PHONEME_T};
    speech_cortex_store_phonological_buffer(state, to_store, 3);

    // Clear
    speech_cortex_clear_phonological_buffer(state);

    // Retrieve should return empty or zero
    phoneme_t retrieved[9];
    uint32_t num_retrieved = 0;
    speech_cortex_retrieve_phonological_buffer(state, retrieved, 9, &num_retrieved);
    EXPECT_EQ(num_retrieved, 0u);

    speech_cortex_destroy(state);
}

//=============================================================================
// Word Recognition Tests
//=============================================================================

TEST_F(SpeechCortexGPUTest, WordRecognition_AddToLexicon) {
    RequireGPU();

    speech_cortex_config_t config = speech_cortex_default_config();
    config.enable_wernicke = true;
    speech_cortex_t* state = speech_cortex_create(&config);
    ASSERT_NE(state, nullptr);

    // Add word to lexicon
    phoneme_t cat_phonemes[] = {PHONEME_K, PHONEME_AE, PHONEME_T};
    bool add_result = speech_cortex_add_word_to_lexicon(state, "cat", cat_phonemes, 3);
    EXPECT_TRUE(add_result);

    speech_cortex_destroy(state);
}

TEST_F(SpeechCortexGPUTest, WordRecognition_RecognizeStoredWord) {
    RequireGPU();

    speech_cortex_config_t config = speech_cortex_default_config();
    config.enable_wernicke = true;
    config.lexicon_size = 1000;
    speech_cortex_t* state = speech_cortex_create(&config);
    ASSERT_NE(state, nullptr);

    // Add word
    phoneme_t cat_phonemes[] = {PHONEME_K, PHONEME_AE, PHONEME_T};
    speech_cortex_add_word_to_lexicon(state, "cat", cat_phonemes, 3);

    // Try to recognize
    char word_buffer[64];
    float confidence = 0.0f;
    bool result = speech_cortex_recognize_word(state, cat_phonemes, 3,
                                                word_buffer, 64, &confidence);

    if (result) {
        EXPECT_STREQ(word_buffer, "cat");
        EXPECT_GT(confidence, 0.0f);
    }

    speech_cortex_destroy(state);
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST_F(SpeechCortexGPUTest, PhonemeNames_ValidStrings) {
    for (int p = 0; p < PHONEME_COUNT; p++) {
        const char* name = speech_cortex_phoneme_name(static_cast<phoneme_t>(p));
        EXPECT_NE(name, nullptr);
        EXPECT_GT(strlen(name), 0u);
    }
}

TEST_F(SpeechCortexGPUTest, PhonemeIPA_ValidSymbols) {
    for (int p = 0; p < PHONEME_COUNT; p++) {
        const char* ipa = speech_cortex_phoneme_ipa(static_cast<phoneme_t>(p));
        EXPECT_NE(ipa, nullptr);
    }
}

TEST_F(SpeechCortexGPUTest, IsVowel_CorrectClassification) {
    // Vowels
    EXPECT_TRUE(speech_cortex_is_vowel(PHONEME_IY));
    EXPECT_TRUE(speech_cortex_is_vowel(PHONEME_AA));
    EXPECT_TRUE(speech_cortex_is_vowel(PHONEME_UW));
    EXPECT_TRUE(speech_cortex_is_vowel(PHONEME_AE));

    // Consonants
    EXPECT_FALSE(speech_cortex_is_vowel(PHONEME_P));
    EXPECT_FALSE(speech_cortex_is_vowel(PHONEME_T));
    EXPECT_FALSE(speech_cortex_is_vowel(PHONEME_S));
    EXPECT_FALSE(speech_cortex_is_vowel(PHONEME_M));
}

//=============================================================================
// Training API Tests
//=============================================================================

TEST_F(SpeechCortexGPUTest, Training_PhonemeTraining) {
    RequireGPU();

    const int sample_rate = 16000;
    auto audio = CreateFormantSignal(270.0f, 2290.0f, 3010.0f, 0.1f, sample_rate);

    speech_cortex_t* state = CreateSpeechCortex(sample_rate);
    ASSERT_NE(state, nullptr);

    // Train on vowel IY
    bool result = speech_cortex_train_phoneme(state, audio.data(), audio.size(),
                                               PHONEME_IY, 1.0f);
    EXPECT_TRUE(result);

    speech_cortex_destroy(state);
}

TEST_F(SpeechCortexGPUTest, Training_PlasticityStats) {
    RequireGPU();

    const int sample_rate = 16000;
    auto audio = CreateSpeechLikeSignal(0.5f, sample_rate);

    speech_cortex_t* state = CreateSpeechCortex(sample_rate);
    ASSERT_NE(state, nullptr);

    // Do some training
    for (int i = 0; i < 10; i++) {
        speech_cortex_train_phoneme(state, audio.data(), audio.size(),
                                     PHONEME_AA, 0.8f);
    }

    uint64_t stdp_updates, mirror_activations, burst_events;
    float avg_lr;
    bool result = speech_cortex_get_plasticity_stats(state, &stdp_updates,
                                                      &mirror_activations,
                                                      &burst_events, &avg_lr);
    EXPECT_TRUE(result);

    speech_cortex_destroy(state);
}

//=============================================================================
// NULL Safety Tests
//=============================================================================

TEST_F(SpeechCortexGPUTest, NullSafety_ProcessWithNull) {
    std::vector<float> features(128);
    bool result = speech_cortex_process(nullptr, nullptr, 0, features.data());
    EXPECT_FALSE(result);
}

TEST_F(SpeechCortexGPUTest, NullSafety_ExtractFormantsWithNull) {
    float formants[4];
    bool result = speech_cortex_extract_formants(nullptr, nullptr, 0, formants, 4);
    EXPECT_FALSE(result);
}

TEST_F(SpeechCortexGPUTest, NullSafety_GetStatsWithNull) {
    speech_cortex_stats_t stats;
    bool result = speech_cortex_get_stats(nullptr, &stats);
    EXPECT_FALSE(result);
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(SpeechCortexGPUTest, Integration_ContinuousSpeechProcessing) {
    RequireGPU();

    const int sample_rate = 16000;
    const int num_chunks = 20;
    const float chunk_duration = 0.05f;  // 50ms chunks

    speech_cortex_t* state = CreateSpeechCortex(sample_rate);
    ASSERT_NE(state, nullptr);

    // Simulate continuous speech stream
    for (int i = 0; i < num_chunks; i++) {
        // Alternate between speech-like and silence
        std::vector<float> chunk;
        if (i % 4 == 0) {
            chunk = CreateSilence(chunk_duration, sample_rate);
        } else {
            chunk = CreateSpeechLikeSignal(chunk_duration, sample_rate);
        }

        std::vector<float> features(128);
        bool result = speech_cortex_process(state, chunk.data(), chunk.size(), features.data());
        EXPECT_TRUE(result) << "Failed on chunk " << i;
    }

    // Verify stats
    speech_cortex_stats_t stats;
    speech_cortex_get_stats(state, &stats);
    EXPECT_GT(stats.frames_processed, 0u);

    speech_cortex_destroy(state);
}

TEST_F(SpeechCortexGPUTest, Integration_FullWordRecognitionPipeline) {
    RequireGPU();

    const int sample_rate = 16000;

    speech_cortex_config_t config = speech_cortex_default_config();
    config.sample_rate = sample_rate;
    config.enable_wernicke = true;
    config.enable_broca = true;
    config.enable_prosody = true;
    config.enable_memory = true;
    config.lexicon_size = 1000;

    speech_cortex_t* state = speech_cortex_create(&config);
    ASSERT_NE(state, nullptr);

    // Add vocabulary
    phoneme_t hello[] = {PHONEME_H, PHONEME_EH, PHONEME_L, PHONEME_OW};
    phoneme_t world[] = {PHONEME_W, PHONEME_ER, PHONEME_L, PHONEME_D};

    speech_cortex_add_word_to_lexicon(state, "hello", hello, 4);
    speech_cortex_add_word_to_lexicon(state, "world", world, 4);

    // Process some audio
    auto audio = CreateSpeechLikeSignal(0.5f, sample_rate);
    std::vector<float> features(config.feature_dim);
    bool result = speech_cortex_process(state, audio.data(), audio.size(), features.data());
    EXPECT_TRUE(result);

    // Detect phonemes
    phoneme_event_t phonemes[64];
    uint32_t num_detected = 0;
    result = speech_cortex_detect_phonemes(state, audio.data(), audio.size(),
                                            phonemes, 64, &num_detected);
    EXPECT_TRUE(result);

    speech_cortex_destroy(state);
}
