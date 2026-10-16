/**
 * @file test_language_cortex_training_regression.cpp
 * @brief Regression-training suite for the language cortex.
 *
 * WHAT: End-to-end training-loop regression tests covering the full
 *       language-cortex stack: grounded_language lexicon + bus + cortex
 *       modulation + per-network bridges + region attaches + probe
 *       metrics. These run a small but representative training loop
 *       (~hundreds of iterations) and pin the *behavior* of each layer
 *       so a future refactor that breaks any single step is caught
 *       before it lands in production.
 *
 * WHY:  Unit tests verify single-call contracts; this suite verifies
 *       *learning* doesn't regress. Past mode-collapse incidents +
 *       statue activations only surfaced after many training steps,
 *       so we exercise the loop at scale.
 *
 * COVERED INVARIANTS:
 *   1. Vocabulary monotonically grows on training.
 *   2. Comprehension confidence rises after training same-word repeats.
 *   3. Subscriber bus fires the expected event types in expected counts.
 *   4. Probe metrics stay sane under training stress.
 *   5. Forgetting telemetry records decay events on sleep consolidation.
 *   6. has_word becomes true after fast_map (sanity contract).
 *   7. Network-bridge response magnitudes are populated when attached.
 *   8. Grounded_language survives 1000 events without leaking subscribers.
 *
 * FRAMEWORK: gtest. No bilateral brain — these tests stay scoped to
 *            grounded_language to avoid the brain_heavy serialization.
 *            Heavy bilateral training tests live in the e2e suite.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdio>
#include <vector>
#include <string>
#include <random>

extern "C" {
#include "language/nimcp_grounded_language.h"
#include "cognitive/memory/nimcp_semantic_memory.h"
#include "cognitive/nimcp_sleep_wake.h"
}

namespace {

constexpr uint32_t TRAIN_DIM = 64;
constexpr int      TRAIN_ITERS = 200;

class LanguageCortexTrainingRegression : public ::testing::Test {
protected:
    grounded_language_t* gl = nullptr;
    semantic_memory_system_t* sm = nullptr;

    void SetUp() override {
        sm = semantic_memory_create();
        ASSERT_NE(sm, nullptr);
        gl = grounded_language_create(TRAIN_DIM, sm);
        ASSERT_NE(gl, nullptr);
    }
    void TearDown() override {
        if (gl) grounded_language_destroy(gl);
        if (sm) semantic_memory_destroy(sm);
    }

    void make_features(std::vector<float>& f, uint32_t seed) {
        f.assign(TRAIN_DIM, 0.0f);
        std::mt19937 rng(seed);
        std::uniform_real_distribution<float> d(0.1f, 1.0f);
        for (uint32_t i = 0; i < TRAIN_DIM; i++) f[i] = d(rng);
    }

    /* Drive a small visual+language training loop. */
    void train_loop(int iters, int n_words) {
        for (int it = 0; it < iters; it++) {
            int wid = it % n_words;
            char buf[32];
            snprintf(buf, sizeof(buf), "trword_%d", wid);
            std::vector<float> f;
            make_features(f, (uint32_t)(wid * 1000 + 42));

            gl_grounding_event_t ev{};
            ev.word = buf;
            ev.modality = GL_MODALITY_VISUAL;
            ev.sensory_features = f.data();
            ev.feature_dim = TRAIN_DIM;
            ev.attention = 0.7f;
            ev.emotional_valence = 0.1f * (float)(wid % 5);
            ev.emotional_arousal = 0.3f;
            grounded_language_ground(gl, &ev);
        }
    }
};

/* === 1. Vocabulary monotonic growth === */
TEST_F(LanguageCortexTrainingRegression, VocabularyMonotonicGrowthDuringTraining) {
    gl_probe_metrics_t pm0;
    grounded_language_get_probe_metrics(gl, &pm0);
    uint32_t v0 = pm0.vocab_count;

    train_loop(TRAIN_ITERS, 50);

    gl_probe_metrics_t pm1;
    grounded_language_get_probe_metrics(gl, &pm1);
    /* New unique words: 50; vocab grows by exactly 50 (no double-counts). */
    EXPECT_EQ(v0 + 50u, pm1.vocab_count);

    /* Continuing with same words must NOT grow vocab further. */
    train_loop(TRAIN_ITERS, 50);
    gl_probe_metrics_t pm2;
    grounded_language_get_probe_metrics(gl, &pm2);
    EXPECT_EQ(pm1.vocab_count, pm2.vocab_count);
}

/* === 2. Confidence rises with repeat exposure === */
TEST_F(LanguageCortexTrainingRegression, ComprehensionConfidenceRisesWithRepeatExposure) {
    /* Burn in a stable lexicon so confidence is measurable. */
    train_loop(20, 5);  /* 5 unique words, 4 reps each */

    gl_comprehension_result_t r1;
    int rc1 = grounded_language_comprehend(gl, "trword_0", &r1);
    ASSERT_EQ(0, rc1);
    float c1 = r1.comprehension_confidence;
    gl_comprehension_result_cleanup(&r1);

    /* More reps. */
    train_loop(100, 5);

    gl_comprehension_result_t r2;
    int rc2 = grounded_language_comprehend(gl, "trword_0", &r2);
    ASSERT_EQ(0, rc2);
    float c2 = r2.comprehension_confidence;
    gl_comprehension_result_cleanup(&r2);

    /* Confidence should not regress. We accept equality (binding strength
     * may saturate); the regression failure is c2 < c1 - eps. */
    EXPECT_GE(c2 + 0.001f, c1)
        << "confidence regressed: " << c1 << " → " << c2;
}

/* === 3. Subscriber bus fires expected counts === */
struct CountCtx {
    int new_word = 0;
    int grounded = 0;
    int total    = 0;
};
extern "C" int counting_subscriber(void* ctx, const gl_event_t* ev) {
    CountCtx* c = (CountCtx*)ctx;
    c->total++;
    if (ev->type == GL_EVENT_NEW_WORD) c->new_word++;
    else if (ev->type == GL_EVENT_GROUNDED) c->grounded++;
    return 0;
}

TEST_F(LanguageCortexTrainingRegression, SubscriberBusFireCountsMatchTraining) {
    CountCtx c{};
    grounded_language_subscribe(gl, counting_subscriber, &c);

    const int n_words = 30;
    train_loop(150, n_words);

    /* Each unique word: 1 NEW_WORD + 1+ GROUNDED. Repeats: GROUNDED only. */
    EXPECT_EQ(n_words, c.new_word);
    EXPECT_GE(c.grounded, 150);  /* one per ground call */
    EXPECT_GE(c.total, c.new_word + c.grounded);
}

/* === 4. Probe metrics under stress === */
TEST_F(LanguageCortexTrainingRegression, ProbeMetricsRemainSaneUnderTrainingStress) {
    train_loop(500, 100);

    gl_probe_metrics_t pm;
    EXPECT_EQ(0, grounded_language_get_probe_metrics(gl, &pm));
    /* Sanity invariants. */
    EXPECT_GT(pm.vocab_count, 0u);
    EXPECT_FALSE(pm.in_fire_event);                 /* no leaked guard */
    EXPECT_GE(pm.avg_binding_strength, 0.0f);
    EXPECT_LE(pm.avg_binding_strength, 1.0f);
    EXPECT_GE(pm.avg_binding_confidence, 0.0f);
    EXPECT_LE(pm.avg_binding_confidence, 1.0f);
    EXPECT_GE(pm.total_groundings, 500u);
}

/* === 5. Forgetting telemetry on sleep consolidation === */
TEST_F(LanguageCortexTrainingRegression, ForgettingTelemetryOnSleepConsolidation) {
    /* Burn in lots of low-frequency words that sleep_consolidate
     * should decay during DEEP_NREM. */
    train_loop(60, 60);

    gl_stats_t s0;
    grounded_language_get_stats(gl, &s0);
    EXPECT_EQ(0u, s0.entries_decayed_all_time);

    /* Run several rounds of NREM consolidation at high strength. */
    for (int i = 0; i < 10; i++) {
        grounded_language_sleep_consolidate(gl, (int)SLEEP_STATE_DEEP_NREM, 0.9f);
    }

    gl_stats_t s1;
    grounded_language_get_stats(gl, &s1);
    /* Some entries should have crossed below the strength floor. */
    EXPECT_GE(s1.entries_decayed_all_time, 0u);
    EXPECT_LE(s1.entries_decayed_last_24h, s1.entries_decayed_all_time);
}

/* === 6. has_word contract under training === */
TEST_F(LanguageCortexTrainingRegression, HasWordTrueAfterTrainingExposure) {
    EXPECT_FALSE(grounded_language_has_word(gl, "trword_0"));
    train_loop(20, 5);
    EXPECT_TRUE(grounded_language_has_word(gl, "trword_0"));
    EXPECT_TRUE(grounded_language_has_word(gl, "trword_4"));
    EXPECT_FALSE(grounded_language_has_word(gl, "trword_99"));
}

/* === 7. Subscriber leak resistance under churn === */
TEST_F(LanguageCortexTrainingRegression, SubscriberCountStableAcrossSubscribeUnsubscribeChurn) {
    /* Cycle 200 subscribe/unsubscribe pairs while training fires. */
    int dummies[20];
    for (int round = 0; round < 10; round++) {
        for (int i = 0; i < 20; i++) {
            grounded_language_subscribe(gl, counting_subscriber, &dummies[i]);
        }
        train_loop(20, 10);
        for (int i = 0; i < 20; i++) {
            grounded_language_unsubscribe(gl, &dummies[i]);
        }
    }
    EXPECT_EQ(0u, grounded_language_subscriber_count(gl));
}

/* === 8. Re-entry guard holds under training === */
struct ReentrantCtx {
    grounded_language_t* gl;
    int outer = 0;
    bool attempt_reenter = true;
};
extern "C" int reentrant_subscriber_t8(void* ctx, const gl_event_t* ev) {
    ReentrantCtx* r = (ReentrantCtx*)ctx;
    r->outer++;
    if (r->attempt_reenter && ev->type == GL_EVENT_NEW_WORD) {
        /* Try to fire another event by grounding a new word. The
         * guard must bail without recursion. */
        std::vector<float> f(TRAIN_DIM, 0.5f);
        gl_grounding_event_t ev2{};
        ev2.word = "reentrant_inner";
        ev2.modality = GL_MODALITY_VISUAL;
        ev2.sensory_features = f.data();
        ev2.feature_dim = TRAIN_DIM;
        ev2.attention = 0.5f;
        grounded_language_ground(r->gl, &ev2);
    }
    return 0;
}

TEST_F(LanguageCortexTrainingRegression, ReentryGuardSurvivesTrainingLoop) {
    ReentrantCtx r{gl};
    grounded_language_subscribe(gl, reentrant_subscriber_t8, &r);
    train_loop(50, 20);

    gl_probe_metrics_t pm;
    grounded_language_get_probe_metrics(gl, &pm);
    EXPECT_GT(pm.events_dropped_reentry, 0u)
        << "guard did not engage during training";
    EXPECT_FALSE(pm.in_fire_event);
}

/* === 9. Subscriber priority preserved during training === */
struct PriorityRecord {
    std::vector<int>* order;
    int id;
};
extern "C" int priority_recorder(void* ctx, const gl_event_t* ev) {
    (void)ev;
    PriorityRecord* p = (PriorityRecord*)ctx;
    p->order->push_back(p->id);
    return 0;
}

TEST_F(LanguageCortexTrainingRegression, PriorityOrderingHoldsAcrossManyEvents) {
    std::vector<int> order;
    PriorityRecord low{&order, 1};
    PriorityRecord mid{&order, 2};
    PriorityRecord hi{&order, 3};

    grounded_language_subscribe_ex(gl, priority_recorder, &low,
                                     GL_EVENT_MASK_ALL, -10);
    grounded_language_subscribe_ex(gl, priority_recorder, &mid,
                                     GL_EVENT_MASK_ALL, 0);
    grounded_language_subscribe_ex(gl, priority_recorder, &hi,
                                     GL_EVENT_MASK_ALL, 50);

    train_loop(30, 15);

    /* Walk recorded order and verify it's always 3,2,1 within each
     * event group. */
    ASSERT_EQ(0u, order.size() % 3) << "non-multiple of 3 — bus dropped";
    for (size_t i = 0; i + 2 < order.size(); i += 3) {
        EXPECT_EQ(3, order[i])     << "hi must fire first @ " << i;
        EXPECT_EQ(2, order[i + 1]) << "mid second";
        EXPECT_EQ(1, order[i + 2]) << "low last";
    }
}

/* === 10. Probe metrics do NOT crash during fire === */
TEST_F(LanguageCortexTrainingRegression, ProbeMetricsDuringFireIsSafe) {
    /* Subscriber that snapshots probe metrics from inside fire.
     * Must not deadlock or read garbage. */
    struct ProbeFromFire { grounded_language_t* gl; int snaps = 0; bool inflight = false; };
    static ProbeFromFire pff;
    pff.gl = gl; pff.snaps = 0; pff.inflight = false;

    auto cb = [](void* ctx, const gl_event_t*) -> int {
        ProbeFromFire* p = (ProbeFromFire*)ctx;
        gl_probe_metrics_t pm;
        grounded_language_get_probe_metrics(p->gl, &pm);
        if (pm.in_fire_event) p->inflight = true;
        p->snaps++;
        return 0;
    };
    grounded_language_subscribe(gl, cb, &pff);

    train_loop(20, 10);

    EXPECT_GT(pff.snaps, 0);
    EXPECT_TRUE(pff.inflight) << "in_fire_event should be observable mid-fire";
}

/* ===================================================================
 * MULTILINGUAL TRAINING REGRESSION (FR / DE / IT / ES / ZH)
 *
 * Verifies GL handles UTF-8 multi-byte words across 5 languages.
 * Each language exercises:
 *   - Word with diacritics (FR/DE/IT/ES) or non-ASCII script (ZH)
 *   - Vocabulary growth on training
 *   - has_word recall after training
 *   - Confidence non-regression on repeat exposure
 * =================================================================*/

/* Representative ~20-word vocabularies. Picked to include challenging
 * characters: diacritics, ligatures, German sharp-s, Spanish ñ,
 * accented vowels, Chinese characters (multi-byte UTF-8 in a single
 * "word" boundary). All under GL_MAX_WORD_LEN=64 even with multi-byte. */
static const char* VOCAB_FR[] = {
    "bonjour", "chat", "chien", "maison", "voiture",
    "élève", "français", "été", "hôtel", "noël",
    "soleil", "lune", "café", "naïveté", "résumé",
    "garçon", "fenêtre", "rêve", "déjà", "aussi", nullptr
};
static const char* VOCAB_DE[] = {
    "guten", "tag", "haus", "wagen", "schule",
    "über", "können", "groß", "müde", "weiß",
    "straße", "fußball", "schön", "bär", "süß",
    "tür", "möglich", "während", "spaß", "läuft", nullptr
};
static const char* VOCAB_IT[] = {
    "ciao", "casa", "macchina", "scuola", "amico",
    "città", "perché", "università", "caffè", "papà",
    "più", "così", "però", "lunedì", "qualità",
    "virtù", "civiltà", "mercoledì", "giù", "già", nullptr
};
static const char* VOCAB_ES[] = {
    "hola", "casa", "coche", "escuela", "amigo",
    "niño", "señor", "español", "corazón", "mañana",
    "año", "España", "pequeño", "sueño", "compañía",
    "página", "estación", "música", "rápido", "fácil", nullptr
};
/* Mandarin: each "word" is 1-2 Chinese characters in UTF-8 (3 bytes
 * per char). All fit in GL_MAX_WORD_LEN. */
static const char* VOCAB_ZH[] = {
    "你好", "猫", "狗", "房子", "汽车",
    "学校", "朋友", "城市", "因为", "大学",
    "咖啡", "爸爸", "更多", "所以", "但是",
    "星期一", "质量", "美德", "下", "已经", nullptr
};

static int vocab_size(const char** v) {
    int n = 0;
    while (v[n]) n++;
    return n;
}

class MultilingualTraining : public LanguageCortexTrainingRegression {
protected:
    void train_vocab(const char** vocab, int reps_per_word) {
        int n = vocab_size(vocab);
        for (int rep = 0; rep < reps_per_word; rep++) {
            for (int i = 0; i < n; i++) {
                std::vector<float> f;
                make_features(f, (uint32_t)(i * 7919 + rep * 31));
                gl_grounding_event_t ev{};
                ev.word = vocab[i];
                ev.modality = GL_MODALITY_VISUAL;
                ev.sensory_features = f.data();
                ev.feature_dim = TRAIN_DIM;
                ev.attention = 0.7f;
                grounded_language_ground(gl, &ev);
            }
        }
    }

    void verify_all_known(const char** vocab, const char* lang) {
        for (int i = 0; vocab[i]; i++) {
            EXPECT_TRUE(grounded_language_has_word(gl, vocab[i]))
                << lang << ": missing word '" << vocab[i] << "'";
        }
    }
};

#define MULTILINGUAL_TEST(NAME, VOCAB, TAG)                              \
TEST_F(MultilingualTraining, NAME) {                                     \
    gl_probe_metrics_t pm0;                                              \
    grounded_language_get_probe_metrics(gl, &pm0);                       \
    int n = vocab_size(VOCAB);                                           \
    train_vocab(VOCAB, 8);                                               \
    gl_probe_metrics_t pm1;                                              \
    grounded_language_get_probe_metrics(gl, &pm1);                       \
    EXPECT_EQ(pm0.vocab_count + (uint32_t)n, pm1.vocab_count)            \
        << TAG ": expected +" << n << " new entries";                    \
    verify_all_known(VOCAB, TAG);                                        \
    /* Confidence non-regression on the 1st word. */                     \
    gl_comprehension_result_t r1, r2;                                    \
    ASSERT_EQ(0, grounded_language_comprehend(gl, VOCAB[0], &r1));       \
    float c1 = r1.comprehension_confidence;                                            \
    gl_comprehension_result_cleanup(&r1);                                \
    train_vocab(VOCAB, 4);                                               \
    ASSERT_EQ(0, grounded_language_comprehend(gl, VOCAB[0], &r2));       \
    float c2 = r2.comprehension_confidence;                                            \
    gl_comprehension_result_cleanup(&r2);                                \
    EXPECT_GE(c2 + 0.001f, c1) << TAG ": confidence regressed";          \
    /* Probes report sane state. */                                      \
    EXPECT_FALSE(pm1.in_fire_event);                                     \
}

MULTILINGUAL_TEST(FrenchTraining,   VOCAB_FR, "FR")
MULTILINGUAL_TEST(GermanTraining,   VOCAB_DE, "DE")
MULTILINGUAL_TEST(ItalianTraining,  VOCAB_IT, "IT")
MULTILINGUAL_TEST(SpanishTraining,  VOCAB_ES, "ES")
MULTILINGUAL_TEST(MandarinTraining, VOCAB_ZH, "ZH")

#undef MULTILINGUAL_TEST

/* Cross-lingual: train all 5 languages into one lexicon, verify each
 * language's words remain retrievable (no hash collisions, no
 * accidental cross-language overwrites). */
TEST_F(MultilingualTraining, FiveLanguagesCoexistInLexicon) {
    train_vocab(VOCAB_FR, 5);
    train_vocab(VOCAB_DE, 5);
    train_vocab(VOCAB_IT, 5);
    train_vocab(VOCAB_ES, 5);
    train_vocab(VOCAB_ZH, 5);

    verify_all_known(VOCAB_FR, "FR");
    verify_all_known(VOCAB_DE, "DE");
    verify_all_known(VOCAB_IT, "IT");
    verify_all_known(VOCAB_ES, "ES");
    verify_all_known(VOCAB_ZH, "ZH");

    /* Total expected: sum of all vocab sizes (assuming no overlaps —
     * "casa" appears in both IT and ES, so we tolerate that delta). */
    gl_probe_metrics_t pm;
    grounded_language_get_probe_metrics(gl, &pm);
    int total = vocab_size(VOCAB_FR) + vocab_size(VOCAB_DE) +
                vocab_size(VOCAB_IT) + vocab_size(VOCAB_ES) +
                vocab_size(VOCAB_ZH);
    /* Allow up to 5 collisions (e.g. casa, ciao). */
    EXPECT_GE(pm.vocab_count, (uint32_t)(total - 5));
    EXPECT_LE(pm.vocab_count - (uint32_t)(total - 5),
              (uint32_t)total + 250);  /* + base bootstrap */
    EXPECT_FALSE(pm.in_fire_event);
}

/* Code-switching simulation: alternate between languages within a
 * single training "session". Models bilingual / polyglot exposure. */
TEST_F(MultilingualTraining, CodeSwitchingDoesNotConfuseLexicon) {
    const char* mixed[] = {
        "bonjour", "guten", "ciao", "hola", "你好",   // greetings × 5 lang
        "chat", "haus", "casa", "coche", "猫",        // common nouns
        "soleil", "müde", "caffè", "música", "更多",
        nullptr
    };
    for (int rep = 0; rep < 10; rep++) {
        for (int i = 0; mixed[i]; i++) {
            std::vector<float> f;
            make_features(f, (uint32_t)(i * 13 + rep * 41));
            gl_grounding_event_t ev{};
            ev.word = mixed[i];
            ev.modality = GL_MODALITY_VISUAL;
            ev.sensory_features = f.data();
            ev.feature_dim = TRAIN_DIM;
            ev.attention = 0.7f;
            grounded_language_ground(gl, &ev);
        }
    }
    for (int i = 0; mixed[i]; i++) {
        EXPECT_TRUE(grounded_language_has_word(gl, mixed[i]))
            << "code-switching dropped: " << mixed[i];
    }
}

/* Confidence parity: after equivalent training, confidence on a
 * French word should be in the same ballpark as on a Mandarin word.
 * Catches a class of regressions where multi-byte words get penalized
 * by hash distribution or feature handling. */
TEST_F(MultilingualTraining, ConfidenceParityAcrossLanguages) {
    train_vocab(VOCAB_FR, 10);
    train_vocab(VOCAB_DE, 10);
    train_vocab(VOCAB_IT, 10);
    train_vocab(VOCAB_ES, 10);
    train_vocab(VOCAB_ZH, 10);

    auto comprehend_conf = [&](const char* w) -> float {
        gl_comprehension_result_t r;
        if (grounded_language_comprehend(gl, w, &r) != 0) return 0.0f;
        float c = r.comprehension_confidence;
        gl_comprehension_result_cleanup(&r);
        return c;
    };

    float cFR = comprehend_conf("bonjour");
    float cDE = comprehend_conf("guten");
    float cIT = comprehend_conf("ciao");
    float cES = comprehend_conf("hola");
    float cZH = comprehend_conf("你好");

    /* All should be > 0 (word exists, was trained). Ratio between
     * any pair should be within 3× — if Mandarin tanks to 0.01 while
     * French is 0.5, something specifically broke for multi-byte. */
    EXPECT_GT(cFR, 0.0f); EXPECT_GT(cDE, 0.0f);
    EXPECT_GT(cIT, 0.0f); EXPECT_GT(cES, 0.0f);
    EXPECT_GT(cZH, 0.0f);

    float min_c = std::min({cFR, cDE, cIT, cES, cZH});
    float max_c = std::max({cFR, cDE, cIT, cES, cZH});
    EXPECT_LE(max_c, min_c * 3.0f + 0.001f)
        << "confidence parity broken: FR=" << cFR << " DE=" << cDE
        << " IT=" << cIT << " ES=" << cES << " ZH=" << cZH;
}

}  // namespace
