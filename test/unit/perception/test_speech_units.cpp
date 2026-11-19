/**
 * @file test_speech_units.cpp
 * @brief Learned sound units, speech-cortex phoneme detection through them,
 *        and the multimodal bridge's learned spoken-word lexicon.
 *
 * Audio is synthetic: each "word" is a sequence of pure tones, so which
 * frames should share a sound category is known in advance.
 */
#include <gtest/gtest.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <unistd.h>

extern "C" {
#include "perception/nimcp_speech_units.h"
#include "perception/nimcp_speech_cortex.h"
#include "nlp/nimcp_multimodal_nlp_bridge.h"
}

// Both features read NIMCP_SPEECH_UNITS once per process; set it first.
static const int kEnv = (setenv("NIMCP_SPEECH_UNITS", "1", 1), unsetenv("NIMCP_SPEECH_UNITS_PATH"), 0);

static const uint32_t SR = 16000;

// Concatenate tones (Hz, ms) with a little noise so frames are not identical.
static std::vector<float> tones(const std::vector<std::pair<float, int>>& parts, unsigned seed) {
    std::vector<float> a;
    srand(seed);
    for (auto& p : parts) {
        int n = (int)(SR * p.second / 1000);
        for (int i = 0; i < n; i++) {
            float noise = 0.01f * ((float)rand() / RAND_MAX - 0.5f);
            float s = p.first > 0 ? 0.5f * sinf(2.0f * (float)M_PI * p.first * (float)i / SR) : 0.0f;
            a.push_back(s + noise);
        }
    }
    return a;
}

static std::vector<uint16_t> units_of(speech_units_t* su, const std::vector<float>& a, bool learn) {
    speech_unit_seg_t segs[64];
    int n = speech_units_segment(su, a.data(), (uint32_t)a.size(), learn, segs, 64);
    std::vector<uint16_t> u;
    for (int i = 0; i < n; i++) u.push_back(segs[i].unit);
    return u;
}

TEST(SpeechUnits, CreateRejectsBadArgs) {
    EXPECT_EQ(speech_units_create(4000, 44), nullptr);
    EXPECT_EQ(speech_units_create(16000, 1), nullptr);
    EXPECT_EQ(speech_units_create(16000, 1000), nullptr);
    speech_units_t* su = speech_units_create(16000, 44);
    ASSERT_NE(su, nullptr);
    EXPECT_EQ(speech_units_num_units(su), 44u);
    EXPECT_EQ(speech_units_num_seeded(su), 0u);
    speech_units_destroy(su);
    speech_units_destroy(nullptr);
}

TEST(SpeechUnits, SilenceAndShortInputGiveNoUnits) {
    speech_units_t* su = speech_units_create(SR, 44);
    std::vector<float> silence(SR, 0.0f);
    speech_unit_seg_t segs[8];
    EXPECT_EQ(speech_units_segment(su, silence.data(), SR, true, segs, 8), 0);
    std::vector<float> tiny(100, 0.3f);
    EXPECT_EQ(speech_units_segment(su, tiny.data(), 100, true, segs, 8), 0);
    EXPECT_EQ(speech_units_segment(su, nullptr, 100, true, segs, 8), -1);
    speech_units_destroy(su);
}

TEST(SpeechUnits, DistinctSoundsGetDistinctStableCategories) {
    speech_units_t* su = speech_units_create(SR, 44);
    // Train on a few clips of low-tone / high-tone.
    for (unsigned s = 0; s < 5; s++) units_of(su, tones({{300, 300}, {2500, 300}}, s), true);
    auto a = units_of(su, tones({{300, 300}, {2500, 300}}, 100), false);
    auto b = units_of(su, tones({{2500, 300}, {300, 300}}, 101), false);
    ASSERT_GE(a.size(), 2u);
    ASSERT_GE(b.size(), 2u);
    // The low tone maps to one category and the high tone to another,
    // in either order.
    EXPECT_NE(a.front(), a.back());
    EXPECT_EQ(a.front(), b.back());
    EXPECT_EQ(a.back(), b.front());
    EXPECT_GE(speech_units_num_seeded(su), 2u);
    EXPECT_GT(speech_units_frames_learned(su), 0u);
    speech_units_destroy(su);
}

TEST(SpeechUnits, SaveLoadReproducesAssignments) {
    speech_units_t* su = speech_units_create(SR, 44);
    for (unsigned s = 0; s < 4; s++) units_of(su, tones({{400, 250}, {1800, 250}, {900, 250}}, s), true);
    auto clip = tones({{400, 250}, {1800, 250}, {900, 250}}, 77);
    auto before = units_of(su, clip, false);
    char path[] = "/tmp/speech_units_XXXXXX";
    int fd = mkstemp(path); ASSERT_GE(fd, 0); close(fd);
    ASSERT_EQ(speech_units_save(su, path), 0);
    speech_units_t* su2 = speech_units_create(SR, 44);
    ASSERT_EQ(speech_units_load(su2, path), 0);
    EXPECT_EQ(speech_units_num_seeded(su2), speech_units_num_seeded(su));
    EXPECT_EQ(units_of(su2, clip, false), before);
    speech_units_t* wrong = speech_units_create(8000, 44);   // other sample rate
    EXPECT_EQ(speech_units_load(wrong, path), -1);
    remove(path);
    speech_units_destroy(su); speech_units_destroy(su2); speech_units_destroy(wrong);
}

TEST(SpeechCortexUnits, DetectPhonemesReturnsLearnedUnitsWhenEnabled) {
    ASSERT_EQ(kEnv, 0);
    speech_cortex_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.sample_rate = SR;
    cfg.num_phonemes = SPEECH_NUM_PHONEMES;
    speech_cortex_t* sc = speech_cortex_create(&cfg);
    ASSERT_NE(sc, nullptr);
    auto clip = tones({{300, 300}, {2500, 300}}, 5);
    phoneme_event_t ev[16];
    uint32_t nd = 0;
    ASSERT_TRUE(speech_cortex_detect_phonemes(sc, clip.data(), (uint32_t)clip.size(), ev, 16, &nd));
    EXPECT_GE(nd, 2u);                       // was always 0 (stub)
    for (uint32_t i = 0; i < nd; i++) {
        EXPECT_LT((int)ev[i].phoneme, SPEECH_NUM_PHONEMES);
        EXPECT_LE(ev[i].onset_time_ms, ev[i].offset_time_ms);
        EXPECT_EQ(ev[i].position_embedding, nullptr);
    }
    EXPECT_GE(speech_cortex_units_seeded(sc), 2u);
    speech_cortex_destroy(sc);
}

static std::vector<phoneme_t> P(std::initializer_list<int> v) {
    std::vector<phoneme_t> o;
    for (int x : v) o.push_back((phoneme_t)x);
    return o;
}

TEST(MultimodalLexicon, LearnsAndRecognisesNearestWord) {
    multimodal_nlp_lexicon_t* lex = multimodal_nlp_lexicon_create(16, 4);
    char w[48]; float c = 0;
    auto up = P({1, 2, 3}), down = P({7, 8, 9, 10});
    EXPECT_EQ(multimodal_nlp_lexicon_recognize(lex, up.data(), 3, w, sizeof w, &c), 0);  // empty
    ASSERT_EQ(multimodal_nlp_lexicon_learn(lex, up.data(), 3, "up"), 0);
    ASSERT_EQ(multimodal_nlp_lexicon_learn(lex, down.data(), 4, "down"), 0);
    auto noisy_up = P({1, 2, 4});
    ASSERT_EQ(multimodal_nlp_lexicon_recognize(lex, noisy_up.data(), 3, w, sizeof w, &c), 1);
    EXPECT_STREQ(w, "up");
    EXPECT_NEAR(c, 2.0f / 3.0f, 1e-5f);
    auto noisy_down = P({7, 8, 10});
    ASSERT_EQ(multimodal_nlp_lexicon_recognize(lex, noisy_down.data(), 3, w, sizeof w, &c), 1);
    EXPECT_STREQ(w, "down");
    EXPECT_EQ(multimodal_nlp_lexicon_num_words(lex), 2u);
    EXPECT_EQ(multimodal_nlp_lexicon_learn(lex, up.data(), 0, "up"), -1);
    EXPECT_EQ(multimodal_nlp_lexicon_learn(lex, up.data(), 3, ""), -1);
    multimodal_nlp_lexicon_destroy(lex);
}

TEST(MultimodalLexicon, SaveLoadKeepsMultiWordEntries) {
    multimodal_nlp_lexicon_t* lex = multimodal_nlp_lexicon_create(16, 4);
    auto a = P({1, 2, 3}), b = P({9, 9, 8});
    multimodal_nlp_lexicon_learn(lex, a.data(), 3, "big dog");
    multimodal_nlp_lexicon_learn(lex, b.data(), 3, "go");
    char path[] = "/tmp/mm_lex_XXXXXX";
    int fd = mkstemp(path); ASSERT_GE(fd, 0); close(fd);
    ASSERT_EQ(multimodal_nlp_lexicon_save(lex, path), 0);
    multimodal_nlp_lexicon_t* lex2 = multimodal_nlp_lexicon_create(16, 4);
    ASSERT_EQ(multimodal_nlp_lexicon_load(lex2, path), 0);
    char w[48]; float c;
    ASSERT_EQ(multimodal_nlp_lexicon_recognize(lex2, a.data(), 3, w, sizeof w, &c), 1);
    EXPECT_STREQ(w, "big dog");
    EXPECT_FLOAT_EQ(c, 1.0f);
    remove(path);
    multimodal_nlp_lexicon_destroy(lex); multimodal_nlp_lexicon_destroy(lex2);
}

TEST(MultimodalBrain, LabelledClipsAreRecognisedAfterLearning) {
    void* slot = nullptr;
    multimodal_nlp_brain_t* mb = multimodal_nlp_brain_for(&slot);
    ASSERT_NE(mb, nullptr);                         // flag on
    EXPECT_EQ(multimodal_nlp_brain_get(&slot), mb);
    // Two "words" made of different tone patterns; NULL speech cortex ->
    // the bridge's own unit model.
    for (unsigned s = 0; s < 30; s++) {
        auto up = tones({{0, 50}, {300, 250}, {2500, 250}, {0, 50}}, 1000 + s);
        auto dn = tones({{0, 50}, {2500, 250}, {300, 250}, {0, 50}}, 2000 + s);
        multimodal_nlp_brain_on_learn(mb, nullptr, up.data(), (uint32_t)up.size(), "Up ");
        multimodal_nlp_brain_on_learn(mb, nullptr, dn.data(), (uint32_t)dn.size(), "down");
    }
    multimodal_nlp_brain_stats_t s;
    ASSERT_EQ(multimodal_nlp_brain_get_stats(mb, nullptr, &s), 0);
    EXPECT_EQ(s.clips, 60u);
    EXPECT_EQ(s.empty_clips, 0u);
    EXPECT_EQ(s.words, 2u);                          // "Up " normalised to "up"
    EXPECT_GE(s.units_seeded, 2u);
    ASSERT_GE(s.predictions, 50u);
    EXPECT_GT((double)s.hits / s.predictions, 0.9); // clean tones: near-perfect

    // The same staged clip offered twice (two learn steps) counts once.
    auto up = tones({{300, 250}, {2500, 250}}, 9);
    multimodal_nlp_brain_on_learn(mb, nullptr, up.data(), (uint32_t)up.size(), "up");
    multimodal_nlp_brain_on_learn(mb, nullptr, up.data(), (uint32_t)up.size(), "up");
    multimodal_nlp_brain_get_stats(mb, nullptr, &s);
    EXPECT_EQ(s.clips, 61u);
    multimodal_nlp_brain_destroy(mb);
}
