/**
 * @file nimcp_toxicity_ml.c
 * @brief In-process MLP toxicity head — see header for design.
 */
#include "security/nimcp_toxicity_ml.h"

#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"

#include <ctype.h>
#include <math.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOG_MODULE "toxicity_ml"

/* Hyperparameters */
#define TML_LR_DEFAULT     0.01f
#define TML_LR_MIN         0.0001f
#define TML_LR_MAX         0.1f
#define TML_MOMENTUM       0.9f
#define TML_NGRAM_LEN      3        /* char trigrams */
#define TML_WEIGHTS_MAGIC  0x54584D4Cu  /* "TXML" */
#define TML_WEIGHTS_VER    1u

/* Layer parameter sizes */
#define TML_W1_SIZE  (TOXICITY_ML_INPUT_DIM   * TOXICITY_ML_HIDDEN1_DIM)
#define TML_W2_SIZE  (TOXICITY_ML_HIDDEN1_DIM * TOXICITY_ML_HIDDEN2_DIM)
#define TML_W3_SIZE  (TOXICITY_ML_HIDDEN2_DIM * TOXICITY_ML_OUTPUT_DIM)

struct toxicity_ml_classifier {
    /* Weights (row-major: W[out, in] stored as out*in_dim + in) */
    float* w1;  float* b1;  /* [hidden1 x input]   + [hidden1] */
    float* w2;  float* b2;  /* [hidden2 x hidden1] + [hidden2] */
    float* w3;  float* b3;  /* [output  x hidden2] + [output]  */

    /* Momentum buffers (same shapes as weights) */
    float* mw1; float* mb1;
    float* mw2; float* mb2;
    float* mw3; float* mb3;

    /* Scratch buffers (per-call; one set guarded by predict_mutex) */
    float* x;   /* [input]   — current features */
    float* h1;  /* [hidden1] — post-ReLU */
    float* h2;  /* [hidden2] — post-ReLU */
    float* y;   /* [output]  — sigmoid    */

    /* Backprop scratch (only used inside train_step) */
    float* dh1; /* [hidden1] */
    float* dh2; /* [hidden2] */
    float* dy;  /* [output]  */

    /* Sync */
    nimcp_mutex_t* train_mutex;

    /* Stats */
    _Atomic uint64_t total_predictions;
    _Atomic uint64_t total_train_steps;
    float recent_loss_ema;  /* updated under train_mutex */
};

/* ---------- helpers ---------------------------------------------------- */

static float frand_xavier(uint32_t* rng, float fan_in) {
    /* xorshift32 + uniform[-r, r] with r = sqrt(6 / fan_in) */
    uint32_t s = *rng;
    s ^= s << 13; s ^= s >> 17; s ^= s << 5;
    *rng = s;
    float u = ((float)(s & 0x00FFFFFFu)) * (1.0f / 16777216.0f); /* [0,1) */
    float r = sqrtf(6.0f / (fan_in > 0.0f ? fan_in : 1.0f));
    return (u * 2.0f - 1.0f) * r;
}

static void
init_weights(toxicity_ml_classifier_t* ml)
{
    uint32_t rng = 0x9E3779B9u ^ (uint32_t)(uintptr_t)ml;
    for (size_t i = 0; i < TML_W1_SIZE; i++)
        ml->w1[i] = frand_xavier(&rng, (float)TOXICITY_ML_INPUT_DIM);
    for (size_t i = 0; i < TML_W2_SIZE; i++)
        ml->w2[i] = frand_xavier(&rng, (float)TOXICITY_ML_HIDDEN1_DIM);
    for (size_t i = 0; i < TML_W3_SIZE; i++)
        ml->w3[i] = frand_xavier(&rng, (float)TOXICITY_ML_HIDDEN2_DIM);
    /* biases zero */
    memset(ml->b1, 0, TOXICITY_ML_HIDDEN1_DIM * sizeof(float));
    memset(ml->b2, 0, TOXICITY_ML_HIDDEN2_DIM * sizeof(float));
    memset(ml->b3, 0, TOXICITY_ML_OUTPUT_DIM  * sizeof(float));
}

/* FNV-1a 32-bit hash on a span. Stable across machines. */
static uint32_t
fnv1a32(const char* s, size_t n)
{
    uint32_t h = 0x811C9DC5u;
    for (size_t i = 0; i < n; i++) {
        h ^= (uint8_t)tolower((unsigned char)s[i]);
        h *= 0x01000193u;
    }
    return h;
}

/* Feature extraction: zero x, then for every char trigram in the text,
 * bump the bin = fnv1a32(trigram) % INPUT_DIM. Normalizes by max(1,
 * count) so longer texts don't dominate. Trigrams overlap a sliding
 * window of 3 chars; tokens shorter than 3 chars hash whole. */
static void
extract_features(const char* text, float* x)
{
    memset(x, 0, TOXICITY_ML_INPUT_DIM * sizeof(float));
    if (!text || !*text) return;
    size_t L = strlen(text);
    int count = 0;
    if (L < TML_NGRAM_LEN) {
        uint32_t h = fnv1a32(text, L);
        x[h % TOXICITY_ML_INPUT_DIM] += 1.0f;
        count = 1;
    } else {
        for (size_t i = 0; i + TML_NGRAM_LEN <= L; i++) {
            uint32_t h = fnv1a32(text + i, TML_NGRAM_LEN);
            x[h % TOXICITY_ML_INPUT_DIM] += 1.0f;
            count++;
        }
    }
    if (count > 1) {
        float inv = 1.0f / (float)count;
        for (size_t i = 0; i < TOXICITY_ML_INPUT_DIM; i++) x[i] *= inv;
    }
}

static float sigmoidf(float z) { return 1.0f / (1.0f + expf(-z)); }

static void
forward(const toxicity_ml_classifier_t* ml)
{
    /* h1 = ReLU(W1 * x + b1) */
    for (size_t j = 0; j < TOXICITY_ML_HIDDEN1_DIM; j++) {
        float s = ml->b1[j];
        const float* w_row = ml->w1 + (size_t)j * TOXICITY_ML_INPUT_DIM;
        for (size_t i = 0; i < TOXICITY_ML_INPUT_DIM; i++) {
            s += w_row[i] * ml->x[i];
        }
        ml->h1[j] = s > 0.0f ? s : 0.0f;
    }
    /* h2 = ReLU(W2 * h1 + b2) */
    for (size_t j = 0; j < TOXICITY_ML_HIDDEN2_DIM; j++) {
        float s = ml->b2[j];
        const float* w_row = ml->w2 + (size_t)j * TOXICITY_ML_HIDDEN1_DIM;
        for (size_t i = 0; i < TOXICITY_ML_HIDDEN1_DIM; i++) {
            s += w_row[i] * ml->h1[i];
        }
        ml->h2[j] = s > 0.0f ? s : 0.0f;
    }
    /* y = sigmoid(W3 * h2 + b3) */
    for (size_t j = 0; j < TOXICITY_ML_OUTPUT_DIM; j++) {
        float s = ml->b3[j];
        const float* w_row = ml->w3 + (size_t)j * TOXICITY_ML_HIDDEN2_DIM;
        for (size_t i = 0; i < TOXICITY_ML_HIDDEN2_DIM; i++) {
            s += w_row[i] * ml->h2[i];
        }
        ml->y[j] = sigmoidf(s);
    }
}

/* ---------- public API ------------------------------------------------- */

toxicity_ml_classifier_t*
toxicity_ml_create(const char* weights_path)
{
    toxicity_ml_classifier_t* ml =
        (toxicity_ml_classifier_t*)nimcp_calloc(1, sizeof(*ml));
    if (!ml) return NULL;

    ml->w1  = (float*)nimcp_calloc(TML_W1_SIZE, sizeof(float));
    ml->b1  = (float*)nimcp_calloc(TOXICITY_ML_HIDDEN1_DIM, sizeof(float));
    ml->w2  = (float*)nimcp_calloc(TML_W2_SIZE, sizeof(float));
    ml->b2  = (float*)nimcp_calloc(TOXICITY_ML_HIDDEN2_DIM, sizeof(float));
    ml->w3  = (float*)nimcp_calloc(TML_W3_SIZE, sizeof(float));
    ml->b3  = (float*)nimcp_calloc(TOXICITY_ML_OUTPUT_DIM, sizeof(float));
    ml->mw1 = (float*)nimcp_calloc(TML_W1_SIZE, sizeof(float));
    ml->mb1 = (float*)nimcp_calloc(TOXICITY_ML_HIDDEN1_DIM, sizeof(float));
    ml->mw2 = (float*)nimcp_calloc(TML_W2_SIZE, sizeof(float));
    ml->mb2 = (float*)nimcp_calloc(TOXICITY_ML_HIDDEN2_DIM, sizeof(float));
    ml->mw3 = (float*)nimcp_calloc(TML_W3_SIZE, sizeof(float));
    ml->mb3 = (float*)nimcp_calloc(TOXICITY_ML_OUTPUT_DIM, sizeof(float));
    ml->x   = (float*)nimcp_calloc(TOXICITY_ML_INPUT_DIM,   sizeof(float));
    ml->h1  = (float*)nimcp_calloc(TOXICITY_ML_HIDDEN1_DIM, sizeof(float));
    ml->h2  = (float*)nimcp_calloc(TOXICITY_ML_HIDDEN2_DIM, sizeof(float));
    ml->y   = (float*)nimcp_calloc(TOXICITY_ML_OUTPUT_DIM,  sizeof(float));
    ml->dh1 = (float*)nimcp_calloc(TOXICITY_ML_HIDDEN1_DIM, sizeof(float));
    ml->dh2 = (float*)nimcp_calloc(TOXICITY_ML_HIDDEN2_DIM, sizeof(float));
    ml->dy  = (float*)nimcp_calloc(TOXICITY_ML_OUTPUT_DIM,  sizeof(float));

    if (!ml->w1 || !ml->b1 || !ml->w2 || !ml->b2 || !ml->w3 || !ml->b3 ||
        !ml->mw1 || !ml->mb1 || !ml->mw2 || !ml->mb2 || !ml->mw3 || !ml->mb3 ||
        !ml->x || !ml->h1 || !ml->h2 || !ml->y ||
        !ml->dh1 || !ml->dh2 || !ml->dy) {
        toxicity_ml_destroy(ml);
        return NULL;
    }

    ml->train_mutex = nimcp_mutex_create(NULL);
    if (!ml->train_mutex) {
        toxicity_ml_destroy(ml);
        return NULL;
    }

    atomic_init(&ml->total_predictions, 0ULL);
    atomic_init(&ml->total_train_steps, 0ULL);
    ml->recent_loss_ema = 0.0f;

    init_weights(ml);

    /* Try to load saved weights — quiet failure if missing. */
    if (weights_path && *weights_path) {
        FILE* f = fopen(weights_path, "rb");
        if (f) {
            uint32_t magic = 0, ver = 0;
            uint32_t in_dim = 0, h1_dim = 0, h2_dim = 0, out_dim = 0;
            if (fread(&magic, sizeof(magic), 1, f) == 1 &&
                fread(&ver,   sizeof(ver),   1, f) == 1 &&
                fread(&in_dim,  sizeof(in_dim),  1, f) == 1 &&
                fread(&h1_dim,  sizeof(h1_dim),  1, f) == 1 &&
                fread(&h2_dim,  sizeof(h2_dim),  1, f) == 1 &&
                fread(&out_dim, sizeof(out_dim), 1, f) == 1 &&
                magic == TML_WEIGHTS_MAGIC && ver == TML_WEIGHTS_VER &&
                in_dim == TOXICITY_ML_INPUT_DIM &&
                h1_dim == TOXICITY_ML_HIDDEN1_DIM &&
                h2_dim == TOXICITY_ML_HIDDEN2_DIM &&
                out_dim == TOXICITY_ML_OUTPUT_DIM) {
                size_t r = 0;
                r += fread(ml->w1, sizeof(float), TML_W1_SIZE, f);
                r += fread(ml->b1, sizeof(float), TOXICITY_ML_HIDDEN1_DIM, f);
                r += fread(ml->w2, sizeof(float), TML_W2_SIZE, f);
                r += fread(ml->b2, sizeof(float), TOXICITY_ML_HIDDEN2_DIM, f);
                r += fread(ml->w3, sizeof(float), TML_W3_SIZE, f);
                r += fread(ml->b3, sizeof(float), TOXICITY_ML_OUTPUT_DIM, f);
                size_t expected = TML_W1_SIZE + TOXICITY_ML_HIDDEN1_DIM +
                                  TML_W2_SIZE + TOXICITY_ML_HIDDEN2_DIM +
                                  TML_W3_SIZE + TOXICITY_ML_OUTPUT_DIM;
                if (r == expected) {
                    LOG_MODULE_INFO("toxicity_ml",
                                    "loaded weights from %s", weights_path);
                } else {
                    LOG_MODULE_WARN("toxicity_ml",
                                    "weights file %s short — using fresh init",
                                    weights_path);
                    init_weights(ml);
                }
            } else {
                LOG_MODULE_WARN("toxicity_ml",
                                "weights file %s header mismatch — fresh init",
                                weights_path);
            }
            fclose(f);
        }
    }

    LOG_MODULE_INFO("toxicity_ml",
                    "ml head initialized: %d->%d->%d->%d",
                    TOXICITY_ML_INPUT_DIM, TOXICITY_ML_HIDDEN1_DIM,
                    TOXICITY_ML_HIDDEN2_DIM, TOXICITY_ML_OUTPUT_DIM);
    return ml;
}

void
toxicity_ml_destroy(toxicity_ml_classifier_t* ml)
{
    if (!ml) return;
    if (ml->train_mutex) nimcp_mutex_free(ml->train_mutex);
    nimcp_free(ml->w1);  nimcp_free(ml->b1);
    nimcp_free(ml->w2);  nimcp_free(ml->b2);
    nimcp_free(ml->w3);  nimcp_free(ml->b3);
    nimcp_free(ml->mw1); nimcp_free(ml->mb1);
    nimcp_free(ml->mw2); nimcp_free(ml->mb2);
    nimcp_free(ml->mw3); nimcp_free(ml->mb3);
    nimcp_free(ml->x);   nimcp_free(ml->h1);  nimcp_free(ml->h2);  nimcp_free(ml->y);
    nimcp_free(ml->dh1); nimcp_free(ml->dh2); nimcp_free(ml->dy);
    nimcp_free(ml);
}

int
toxicity_ml_predict(const toxicity_ml_classifier_t* ml,
                    const char* text,
                    toxicity_ml_result_t* out)
{
    if (!ml || !out) return -1;
    memset(out, 0, sizeof(*out));
    if (!text || !*text) return 0;

    /* Hold the train_mutex around the forward pass so a concurrent
     * train_step() doesn't read torn weights while writing them. This is
     * correct but blocks throughput; tolerable for the current call
     * rates. (Lock-free predict + double-buffered weights is a future
     * optimization.) */
    toxicity_ml_classifier_t* mlm = (toxicity_ml_classifier_t*)ml;
    nimcp_mutex_lock(mlm->train_mutex);
    extract_features(text, mlm->x);
    forward(mlm);
    out->predicted_harm     = mlm->y[0];
    out->fairness_violation = mlm->y[1];
    float c0 = mlm->y[0] - 0.5f; if (c0 < 0) c0 = -c0;
    float c1 = mlm->y[1] - 0.5f; if (c1 < 0) c1 = -c1;
    out->confidence = (c0 > c1 ? c0 : c1) * 2.0f;
    if (out->confidence > 1.0f) out->confidence = 1.0f;
    nimcp_mutex_unlock(mlm->train_mutex);

    atomic_fetch_add_explicit(&mlm->total_predictions, 1ULL,
                              memory_order_relaxed);
    return 0;
}

float
toxicity_ml_train_step(toxicity_ml_classifier_t* ml,
                       const char* text,
                       float target_harm,
                       float target_fairness,
                       float learning_rate,
                       float dead_zone)
{
    if (!ml || !text || !*text) return NAN;
    if (target_harm < 0.0f) target_harm = 0.0f;
    if (target_harm > 1.0f) target_harm = 1.0f;
    if (target_fairness < 0.0f) target_fairness = 0.0f;
    if (target_fairness > 1.0f) target_fairness = 1.0f;
    if (!(learning_rate > 0.0f)) learning_rate = TML_LR_DEFAULT;
    if (learning_rate < TML_LR_MIN) learning_rate = TML_LR_MIN;
    if (learning_rate > TML_LR_MAX) learning_rate = TML_LR_MAX;
    if (dead_zone < 0.0f) dead_zone = 0.0f;

    nimcp_mutex_lock(ml->train_mutex);
    extract_features(text, ml->x);
    forward(ml);

    float err0 = ml->y[0] - target_harm;
    float err1 = ml->y[1] - target_fairness;
    float pre_loss = 0.5f * (err0 * err0 + err1 * err1);

    /* Dead-zone: if both outputs are already close enough to the target,
     * skip the update (saves churn during normal training cycles). */
    if (fabsf(err0) <= dead_zone && fabsf(err1) <= dead_zone) {
        nimcp_mutex_unlock(ml->train_mutex);
        return pre_loss;
    }

    /* === Backward pass ===
     * Loss: L = 0.5 * sum((y_k - t_k)^2)
     * Output sigmoid: dL/dz_k = (y_k - t_k) * y_k * (1 - y_k)
     */
    for (size_t k = 0; k < TOXICITY_ML_OUTPUT_DIM; k++) {
        float y = ml->y[k];
        float err = (k == 0) ? err0 : err1;
        ml->dy[k] = err * y * (1.0f - y);
    }

    /* dh2[j] = sum_k W3[k,j] * dy[k] * relu'(h2[j]) */
    for (size_t j = 0; j < TOXICITY_ML_HIDDEN2_DIM; j++) {
        float s = 0.0f;
        for (size_t k = 0; k < TOXICITY_ML_OUTPUT_DIM; k++) {
            s += ml->w3[(size_t)k * TOXICITY_ML_HIDDEN2_DIM + j] * ml->dy[k];
        }
        ml->dh2[j] = (ml->h2[j] > 0.0f) ? s : 0.0f;
    }

    /* dh1[j] = sum_k W2[k,j] * dh2[k] * relu'(h1[j]) */
    for (size_t j = 0; j < TOXICITY_ML_HIDDEN1_DIM; j++) {
        float s = 0.0f;
        for (size_t k = 0; k < TOXICITY_ML_HIDDEN2_DIM; k++) {
            s += ml->w2[(size_t)k * TOXICITY_ML_HIDDEN1_DIM + j] * ml->dh2[k];
        }
        ml->dh1[j] = (ml->h1[j] > 0.0f) ? s : 0.0f;
    }

    /* SGD-with-momentum update on each weight matrix + bias.
     * m_t = beta * m_{t-1} + grad
     * w   = w - lr * m_t
     */
    float beta = TML_MOMENTUM;
    /* W3, b3 */
    for (size_t k = 0; k < TOXICITY_ML_OUTPUT_DIM; k++) {
        for (size_t i = 0; i < TOXICITY_ML_HIDDEN2_DIM; i++) {
            size_t idx = k * TOXICITY_ML_HIDDEN2_DIM + i;
            float g = ml->dy[k] * ml->h2[i];
            ml->mw3[idx] = beta * ml->mw3[idx] + g;
            ml->w3[idx] -= learning_rate * ml->mw3[idx];
        }
        ml->mb3[k] = beta * ml->mb3[k] + ml->dy[k];
        ml->b3[k] -= learning_rate * ml->mb3[k];
    }
    /* W2, b2 */
    for (size_t k = 0; k < TOXICITY_ML_HIDDEN2_DIM; k++) {
        for (size_t i = 0; i < TOXICITY_ML_HIDDEN1_DIM; i++) {
            size_t idx = k * TOXICITY_ML_HIDDEN1_DIM + i;
            float g = ml->dh2[k] * ml->h1[i];
            ml->mw2[idx] = beta * ml->mw2[idx] + g;
            ml->w2[idx] -= learning_rate * ml->mw2[idx];
        }
        ml->mb2[k] = beta * ml->mb2[k] + ml->dh2[k];
        ml->b2[k] -= learning_rate * ml->mb2[k];
    }
    /* W1, b1 */
    for (size_t k = 0; k < TOXICITY_ML_HIDDEN1_DIM; k++) {
        for (size_t i = 0; i < TOXICITY_ML_INPUT_DIM; i++) {
            size_t idx = k * TOXICITY_ML_INPUT_DIM + i;
            float g = ml->dh1[k] * ml->x[i];
            ml->mw1[idx] = beta * ml->mw1[idx] + g;
            ml->w1[idx] -= learning_rate * ml->mw1[idx];
        }
        ml->mb1[k] = beta * ml->mb1[k] + ml->dh1[k];
        ml->b1[k] -= learning_rate * ml->mb1[k];
    }

    /* EMA on loss (0.01 mixing) for telemetry. */
    if (ml->recent_loss_ema == 0.0f) ml->recent_loss_ema = pre_loss;
    else ml->recent_loss_ema = 0.99f * ml->recent_loss_ema + 0.01f * pre_loss;

    nimcp_mutex_unlock(ml->train_mutex);
    atomic_fetch_add_explicit(&ml->total_train_steps, 1ULL,
                              memory_order_relaxed);
    return pre_loss;
}

int
toxicity_ml_save(const toxicity_ml_classifier_t* ml, const char* weights_path)
{
    if (!ml || !weights_path) return -1;
    char tmp_path[1024];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", weights_path);
    FILE* f = fopen(tmp_path, "wb");
    if (!f) return -1;

    uint32_t magic = TML_WEIGHTS_MAGIC;
    uint32_t ver = TML_WEIGHTS_VER;
    uint32_t in_dim  = TOXICITY_ML_INPUT_DIM;
    uint32_t h1_dim  = TOXICITY_ML_HIDDEN1_DIM;
    uint32_t h2_dim  = TOXICITY_ML_HIDDEN2_DIM;
    uint32_t out_dim = TOXICITY_ML_OUTPUT_DIM;

    int ok = 1;
    ok = ok && fwrite(&magic, sizeof(magic), 1, f) == 1;
    ok = ok && fwrite(&ver,   sizeof(ver),   1, f) == 1;
    ok = ok && fwrite(&in_dim,  sizeof(in_dim),  1, f) == 1;
    ok = ok && fwrite(&h1_dim,  sizeof(h1_dim),  1, f) == 1;
    ok = ok && fwrite(&h2_dim,  sizeof(h2_dim),  1, f) == 1;
    ok = ok && fwrite(&out_dim, sizeof(out_dim), 1, f) == 1;
    ok = ok && fwrite(ml->w1, sizeof(float), TML_W1_SIZE, f) == TML_W1_SIZE;
    ok = ok && fwrite(ml->b1, sizeof(float), TOXICITY_ML_HIDDEN1_DIM, f) == TOXICITY_ML_HIDDEN1_DIM;
    ok = ok && fwrite(ml->w2, sizeof(float), TML_W2_SIZE, f) == TML_W2_SIZE;
    ok = ok && fwrite(ml->b2, sizeof(float), TOXICITY_ML_HIDDEN2_DIM, f) == TOXICITY_ML_HIDDEN2_DIM;
    ok = ok && fwrite(ml->w3, sizeof(float), TML_W3_SIZE, f) == TML_W3_SIZE;
    ok = ok && fwrite(ml->b3, sizeof(float), TOXICITY_ML_OUTPUT_DIM, f) == TOXICITY_ML_OUTPUT_DIM;
    fclose(f);
    if (!ok) {
        remove(tmp_path);
        return -1;
    }
    if (rename(tmp_path, weights_path) != 0) {
        remove(tmp_path);
        return -1;
    }
    return 0;
}

void
toxicity_ml_get_stats(const toxicity_ml_classifier_t* ml,
                      uint64_t* out_predictions,
                      uint64_t* out_train_steps,
                      float* out_recent_loss)
{
    if (!ml) return;
    toxicity_ml_classifier_t* mlm = (toxicity_ml_classifier_t*)ml;
    if (out_predictions) {
        *out_predictions =
            atomic_load_explicit(&mlm->total_predictions,
                                 memory_order_relaxed);
    }
    if (out_train_steps) {
        *out_train_steps =
            atomic_load_explicit(&mlm->total_train_steps,
                                 memory_order_relaxed);
    }
    if (out_recent_loss) {
        nimcp_mutex_lock(mlm->train_mutex);
        *out_recent_loss = mlm->recent_loss_ema;
        nimcp_mutex_unlock(mlm->train_mutex);
    }
}
