/**
 * @file nimcp_language_generator.c
 * @brief Language generation engine using LNN temporal dynamics
 *
 * WHAT: Full implementation of autoregressive text generation from cognitive state
 * WHY:  Converts reasoning chain outputs into natural language via LNN decoder
 * HOW:  Cognitive projection -> NCP LNN forward steps -> output projection ->
 *       softmax -> strategy-based sampling -> token-by-token decoding
 *
 * IMPLEMENTATION NOTES:
 * - All allocations use nimcp_malloc / nimcp_calloc / nimcp_free
 * - Tokenizer and embedding are accessed via extern function declarations
 *   so we remain decoupled from their headers
 * - Xavier initialisation for weight matrices ensures stable gradient flow
 * - Repetition penalty divides logits of recently-generated tokens
 * - Perplexity = exp( -1/N * sum(log(confidence_i)) )
 *
 * @author NIMCP Development Team
 * @date 2026-02-25
 * @version 1.0.0
 */

#include "generation/nimcp_language_generator.h"
#include "lnn/nimcp_lnn_network.h"
#include "utils/tensor/nimcp_tensor.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <float.h>
#include <time.h>

#ifdef __AVX2__
#include <immintrin.h>
#endif

/*=============================================================================
 * Extern Tokenizer / Embedding Functions
 *
 * WHAT: Forward-declared interfaces to tokenizer and embedding modules
 * WHY:  The generator references these via opaque void*, so we declare the
 *       minimal set of functions we actually call
 * HOW:  Callers must link against libraries that provide these symbols
 *===========================================================================*/

extern uint32_t tokenizer_get_vocab_size(const void* tok);
extern int      tokenizer_encode(const void* tok, const char* text,
                                 uint32_t* ids, uint32_t max_ids,
                                 uint32_t* out_count);
extern int      tokenizer_decode(const void* tok, const uint32_t* ids,
                                 uint32_t count, char* text,
                                 uint32_t max_len);

extern int      embedding_lookup(const void* emb, uint32_t token_id,
                                 float* out_vector);

/*=============================================================================
 * Internal Structure
 *===========================================================================*/

struct language_generator {
    generator_config_t config;

    /* External references (NOT owned) */
    void* tokenizer;
    void* embedding;

    /* LNN decoder network (owned) */
    lnn_network_t* decoder_lnn;

    /* Output projection: hidden_dim -> vocab_size */
    float* output_projection;   /* [hidden_dim * vocab_size] row-major */
    float* output_bias;         /* [vocab_size] */
    uint32_t vocab_size;
    uint32_t embed_dim;

    /* Cognitive state projection: cognitive_dim -> hidden_dim (lazy alloc) */
    float* cognitive_projection; /* [cognitive_dim * hidden_dim] */
    uint32_t cognitive_dim;      /* Set on first call to generate() */

    /* Embedding -> hidden projection: embed_dim -> hidden_dim */
    float* embed_projection;    /* [embed_dim * hidden_dim] */

    /* Repetition tracking (circular buffer) */
    uint32_t* recent_tokens;
    uint32_t  recent_count;
    uint32_t  recent_capacity;

    /* Statistics */
    uint32_t total_generations;
    uint32_t total_tokens_generated;
    float    cumulative_perplexity;

    /* Per-instance PRNG state for thread safety */
    unsigned int rng_seed;
};

/*=============================================================================
 * Static Helpers - Numeric Utilities
 *===========================================================================*/

/**
 * WHAT: In-place numerically-stable softmax over a logit array
 * WHY:  Converts raw logits to a probability distribution
 * HOW:  Subtract max for stability, exponentiate, normalise
 */
static void softmax_inplace(float* logits, uint32_t size) {
    if (!logits || size == 0) return;

    /* Find max for numerical stability */
    float max_val = logits[0];
    for (uint32_t i = 1; i < size; i++) {
        if (logits[i] > max_val) max_val = logits[i];
    }

    /* Exponentiate and accumulate */
    float sum = 0.0f;
    for (uint32_t i = 0; i < size; i++) {
        logits[i] = expf(logits[i] - max_val);
        sum += logits[i];
    }

    /* Normalise */
    if (sum > 0.0f) {
        float inv = 1.0f / sum;
        for (uint32_t i = 0; i < size; i++) {
            logits[i] *= inv;
        }
    }
}

/**
 * WHAT: Sample one index from a categorical distribution
 * WHY:  Core sampling primitive for all stochastic decoding strategies
 * HOW:  Draw uniform random in [0,1), walk cumulative sum until exceeded
 */
static uint32_t sample_categorical(const float* probs, uint32_t size, unsigned int* rng_seed) {
    float r = (float)rand_r(rng_seed) / ((float)RAND_MAX + 1.0f);
    float cumsum = 0.0f;
    for (uint32_t i = 0; i < size; i++) {
        cumsum += probs[i];
        if (r < cumsum) return i;
    }
    return size - 1;
}

/**
 * WHAT: Return the index of the maximum value
 * WHY:  Used by greedy decoding
 */
static uint32_t argmax(const float* arr, uint32_t size) {
    uint32_t best = 0;
    float best_val = arr[0];
    for (uint32_t i = 1; i < size; i++) {
        if (arr[i] > best_val) {
            best_val = arr[i];
            best = i;
        }
    }
    return best;
}

/**
 * WHAT: Xavier / Glorot uniform initialisation for a weight matrix
 * WHY:  Keeps variance stable across layers at init time
 * HOW:  U(-limit, +limit) where limit = sqrt(6 / (fan_in + fan_out))
 */
static void xavier_init(float* weights, uint32_t fan_in, uint32_t fan_out, unsigned int* rng_seed) {
    float limit = sqrtf(6.0f / (float)(fan_in + fan_out));
    uint32_t total = fan_in * fan_out;
    for (uint32_t i = 0; i < total; i++) {
        float u = (float)rand_r(rng_seed) / (float)RAND_MAX;  /* [0, 1] */
        weights[i] = (2.0f * u - 1.0f) * limit;               /* [-limit, +limit] */
    }
}

/**
 * WHAT: Matrix-vector multiply: out = W @ x  (W is [rows x cols], x is [cols])
 * WHY:  Used by cognitive projection and output projection
 * HOW:  Simple row-major dot products
 */
static void matvec(const float* W, const float* x,
                   float* out, uint32_t rows, uint32_t cols) {
#ifdef __AVX2__
    for (uint32_t r = 0; r < rows; r++) {
        const float* row = W + (size_t)r * cols;
        __m256 acc = _mm256_setzero_ps();
        uint32_t c = 0;
        for (; c + 8 <= cols; c += 8) {
            __m256 w = _mm256_loadu_ps(row + c);
            __m256 v = _mm256_loadu_ps(x + c);
            acc = _mm256_fmadd_ps(w, v, acc);
        }
        /* Horizontal sum */
        __m128 hi = _mm256_extractf128_ps(acc, 1);
        __m128 lo = _mm256_castps256_ps128(acc);
        __m128 sum4 = _mm_add_ps(lo, hi);
        __m128 shuf = _mm_movehdup_ps(sum4);
        __m128 sum2 = _mm_add_ss(sum4, shuf);
        shuf = _mm_movehl_ps(shuf, sum2);
        __m128 sum1 = _mm_add_ss(sum2, shuf);
        float result = _mm_cvtss_f32(sum1);
        /* Scalar tail */
        for (; c < cols; c++) {
            result += row[c] * x[c];
        }
        out[r] = result;
    }
#else
    for (uint32_t r = 0; r < rows; r++) {
        float acc = 0.0f;
        const float* row = W + (size_t)r * cols;
        for (uint32_t c = 0; c < cols; c++) {
            acc += row[c] * x[c];
        }
        out[r] = acc;
    }
#endif
}

/**
 * WHAT: Add a bias vector element-wise: out[i] += bias[i]
 * WHY:  Output projection has an additive bias term
 */
static void add_bias(float* out, const float* bias, uint32_t size) {
#ifdef __AVX2__
    uint32_t i = 0;
    for (; i + 8 <= size; i += 8) {
        __m256 o = _mm256_loadu_ps(out + i);
        __m256 b = _mm256_loadu_ps(bias + i);
        _mm256_storeu_ps(out + i, _mm256_add_ps(o, b));
    }
    for (; i < size; i++) {
        out[i] += bias[i];
    }
#else
    for (uint32_t i = 0; i < size; i++) {
        out[i] += bias[i];
    }
#endif
}

/**
 * WHAT: Simple DJB2-style string hash that returns a 32-bit value
 * WHY:  Used to seed feature generation from text (conclusion hashing)
 */
static uint32_t hash_string(const char* str) {
    uint32_t h = 5381;
    if (!str) return h;
    while (*str) {
        h = ((h << 5) + h) + (uint32_t)(unsigned char)*str;
        str++;
    }
    return h;
}

/*=============================================================================
 * Static Helpers - Decoding Strategies
 *===========================================================================*/

/**
 * WHAT: Apply top-k filtering to a probability distribution
 * WHY:  Restricts sampling to the k most-likely tokens
 * HOW:  Find the k-th largest probability, zero out everything below it,
 *       renormalise
 */
static void apply_top_k(float* probs, uint32_t size, uint32_t k) {
    if (k == 0 || k >= size) return;

    /* Find the k-th largest value via partial selection.
     * For simplicity we copy and do a descending partial sort. */
    float* sorted = (float*)nimcp_malloc(size * sizeof(float));
    if (!sorted) return;
    memcpy(sorted, probs, size * sizeof(float));

    /* Partial bubble-down for k iterations (sufficient for typical k<=200) */
    for (uint32_t i = 0; i < k; i++) {
        for (uint32_t j = i + 1; j < size; j++) {
            if (sorted[j] > sorted[i]) {
                float tmp = sorted[i];
                sorted[i] = sorted[j];
                sorted[j] = tmp;
            }
        }
    }
    float threshold = sorted[k - 1];
    nimcp_free(sorted);

    /* Zero out entries below threshold and renormalise */
    float sum = 0.0f;
    for (uint32_t i = 0; i < size; i++) {
        if (probs[i] < threshold) {
            probs[i] = 0.0f;
        } else {
            sum += probs[i];
        }
    }
    if (sum > 0.0f) {
        float inv = 1.0f / sum;
        for (uint32_t i = 0; i < size; i++) probs[i] *= inv;
    }
}

/**
 * WHAT: Apply nucleus (top-p) filtering to a probability distribution
 * WHY:  Dynamically selects the smallest set of tokens whose cumulative
 *       probability exceeds top_p, giving adaptive breadth
 * HOW:  Sort indices by probability descending, accumulate until threshold,
 *       zero out the rest, renormalise
 */
static void apply_top_p(float* probs, uint32_t size, float top_p) {
    if (top_p >= 1.0f) return;

    /* Build index array sorted by probability descending */
    uint32_t* indices = (uint32_t*)nimcp_malloc(size * sizeof(uint32_t));
    if (!indices) return;
    for (uint32_t i = 0; i < size; i++) indices[i] = i;

    /* Insertion sort (stable, and vocab sizes are typically manageable) */
    for (uint32_t i = 1; i < size; i++) {
        uint32_t key = indices[i];
        float key_p = probs[key];
        int32_t j = (int32_t)i - 1;
        while (j >= 0 && probs[indices[j]] < key_p) {
            indices[j + 1] = indices[j];
            j--;
        }
        indices[j + 1] = key;
    }

    /* Walk sorted order, accumulate probability */
    float cumulative = 0.0f;
    uint32_t cutoff = size;
    for (uint32_t i = 0; i < size; i++) {
        cumulative += probs[indices[i]];
        if (cumulative >= top_p) {
            cutoff = i + 1;
            break;
        }
    }

    /* Zero out tokens outside the nucleus */
    for (uint32_t i = cutoff; i < size; i++) {
        probs[indices[i]] = 0.0f;
    }
    nimcp_free(indices);

    /* Renormalise */
    float sum = 0.0f;
    for (uint32_t i = 0; i < size; i++) sum += probs[i];
    if (sum > 0.0f) {
        float inv = 1.0f / sum;
        for (uint32_t i = 0; i < size; i++) probs[i] *= inv;
    }
}

/**
 * WHAT: Apply repetition penalty to logits for recently-generated tokens
 * WHY:  Discourages the model from repeating itself
 * HOW:  For each token in the recent buffer, divide its logit by the penalty
 *       factor (if logit > 0) or multiply (if logit < 0)
 */
static void apply_repetition_penalty(float* logits, uint32_t vocab_size,
                                     const uint32_t* recent, uint32_t count,
                                     float penalty) {
    if (penalty <= 1.0f) return;
    for (uint32_t i = 0; i < count; i++) {
        uint32_t tok = recent[i];
        if (tok >= vocab_size) continue;
        if (logits[tok] > 0.0f) {
            logits[tok] /= penalty;
        } else {
            logits[tok] *= penalty;
        }
    }
}

/**
 * WHAT: Push a token into the circular recent-token buffer
 * WHY:  Maintains a sliding window for repetition penalty
 */
static void push_recent(language_generator_t* gen, uint32_t token_id) {
    if (gen->recent_count < gen->recent_capacity) {
        gen->recent_tokens[gen->recent_count++] = token_id;
    } else {
        /* Shift left by one, append at end */
        memmove(gen->recent_tokens, gen->recent_tokens + 1,
                (gen->recent_capacity - 1) * sizeof(uint32_t));
        gen->recent_tokens[gen->recent_capacity - 1] = token_id;
    }
}

/**
 * WHAT: Clear the recent-token buffer
 * WHY:  Reset repetition tracking between generations
 */
static void clear_recent(language_generator_t* gen) {
    gen->recent_count = 0;
}

/*=============================================================================
 * Static Helpers - Beam Search
 *===========================================================================*/

/** Single beam hypothesis */
typedef struct {
    uint32_t* tokens;        /**< Token sequence so far */
    uint32_t  length;        /**< Current length */
    float     log_prob;      /**< Cumulative log-probability */
    bool      finished;      /**< Reached EOS? */
} beam_t;

/**
 * WHAT: Run beam search decoding
 * WHY:  Explores multiple hypotheses to find higher-probability sequences
 * HOW:  Maintain beam_width partial hypotheses; at each step expand each
 *       beam with the top-k tokens and keep the best beam_width overall
 *
 * NOTE: This function manages its own tensor creation for LNN steps.
 *       The generator's LNN state is reset before beam search starts.
 */
static int beam_search_generate(
    language_generator_t* gen,
    const float* initial_hidden,
    generation_result_t* result)
{
    uint32_t bw = gen->config.beam_width;
    uint32_t max_len = gen->config.max_sequence_length;
    uint32_t vocab = gen->vocab_size;
    uint32_t hdim = gen->config.hidden_dim;

    /* Allocate beams */
    beam_t* beams = (beam_t*)nimcp_calloc(bw, sizeof(beam_t));
    if (!beams) return -1;

    for (uint32_t b = 0; b < bw; b++) {
        beams[b].tokens = (uint32_t*)nimcp_calloc(max_len, sizeof(uint32_t));
        if (!beams[b].tokens) {
            for (uint32_t j = 0; j < b; j++) nimcp_free(beams[j].tokens);
            nimcp_free(beams);
            return -1;
        }
        beams[b].length = 0;
        beams[b].log_prob = 0.0f;
        beams[b].finished = false;
    }

    /* Working buffers */
    float* logits_buf = (float*)nimcp_malloc(vocab * sizeof(float));
    float* hidden_buf = (float*)nimcp_malloc(hdim * sizeof(float));
    float* embed_buf  = (float*)nimcp_malloc(gen->embed_dim * sizeof(float));
    float* proj_buf   = (float*)nimcp_malloc(hdim * sizeof(float));

    if (!logits_buf || !hidden_buf || !embed_buf || !proj_buf) {
        nimcp_free(logits_buf); nimcp_free(hidden_buf);
        nimcp_free(embed_buf); nimcp_free(proj_buf);
        for (uint32_t b = 0; b < bw; b++) nimcp_free(beams[b].tokens);
        nimcp_free(beams);
        return -1;
    }

    /* Candidate buffer for expansion (bw * bw candidates) */
    uint32_t max_cand = bw * bw;
    typedef struct { uint32_t beam; uint32_t token; float score; } cand_t;
    cand_t* cands = (cand_t*)nimcp_malloc(max_cand * sizeof(cand_t));
    if (!cands) {
        nimcp_free(logits_buf); nimcp_free(hidden_buf);
        nimcp_free(embed_buf); nimcp_free(proj_buf);
        for (uint32_t b = 0; b < bw; b++) nimcp_free(beams[b].tokens);
        nimcp_free(beams);
        return -1;
    }

    /* For simplicity the beam search uses the same LNN for all beams,
     * re-running from scratch for each beam at each step.  This is O(bw*t)
     * forward steps per generation step -- acceptable for small beam widths. */

    uint32_t input_dims[1] = {hdim};
    uint32_t output_dims[1] = {hdim};

    for (uint32_t step = 0; step < max_len; step++) {
        uint32_t n_cand = 0;
        bool all_finished = true;

        for (uint32_t b = 0; b < bw; b++) {
            if (beams[b].finished) continue;
            all_finished = false;

            /* Re-run LNN from initial hidden for this beam's token sequence */
            lnn_network_reset_state(gen->decoder_lnn);

            nimcp_tensor_t* inp = nimcp_tensor_create(input_dims, 1, NIMCP_DTYPE_F32);
            nimcp_tensor_t* outp = nimcp_tensor_create(output_dims, 1, NIMCP_DTYPE_F32);
            if (!inp || !outp) {
                nimcp_tensor_destroy(inp);
                nimcp_tensor_destroy(outp);
                continue;
            }

            /* Seed with initial hidden */
            memcpy(nimcp_tensor_data(inp), initial_hidden, hdim * sizeof(float));
            lnn_network_forward_step(gen->decoder_lnn, inp, outp, 1.0f);

            /* Replay beam tokens */
            for (uint32_t t = 0; t < beams[b].length; t++) {
                embedding_lookup(gen->embedding, beams[b].tokens[t],
                                 embed_buf);
                matvec(gen->embed_projection, embed_buf, proj_buf,
                       hdim, gen->embed_dim);
                memcpy(nimcp_tensor_data(inp), proj_buf, hdim * sizeof(float));
                lnn_network_forward_step(gen->decoder_lnn, inp, outp, 1.0f);
            }

            /* Get hidden output */
            memcpy(hidden_buf, nimcp_tensor_data_const(outp), hdim * sizeof(float));
            nimcp_tensor_destroy(inp);
            nimcp_tensor_destroy(outp);

            /* Compute logits */
            matvec(gen->output_projection, hidden_buf, logits_buf, vocab, hdim);
            add_bias(logits_buf, gen->output_bias, vocab);

            /* Temperature */
            if (gen->config.temperature > 0.0f && gen->config.temperature != 1.0f) {
                float inv_t = 1.0f / gen->config.temperature;
                for (uint32_t i = 0; i < vocab; i++) logits_buf[i] *= inv_t;
            }

            /* Softmax to get log probs */
            softmax_inplace(logits_buf, vocab);

            /* Pick top-bw tokens for this beam */
            for (uint32_t k = 0; k < bw && n_cand < max_cand; k++) {
                uint32_t best_idx = argmax(logits_buf, vocab);
                float p = logits_buf[best_idx];
                if (p <= 0.0f) break;
                cands[n_cand].beam = b;
                cands[n_cand].token = best_idx;
                cands[n_cand].score = beams[b].log_prob + logf(p + 1e-10f);
                n_cand++;
                logits_buf[best_idx] = 0.0f; /* mask for next iteration */
            }
        }

        if (all_finished || n_cand == 0) break;

        /* Sort candidates by score descending */
        for (uint32_t i = 0; i < n_cand; i++) {
            for (uint32_t j = i + 1; j < n_cand; j++) {
                if (cands[j].score > cands[i].score) {
                    cand_t tmp = cands[i];
                    cands[i] = cands[j];
                    cands[j] = tmp;
                }
            }
        }

        /* Build new beams from top-bw candidates */
        beam_t* new_beams = (beam_t*)nimcp_calloc(bw, sizeof(beam_t));
        if (!new_beams) break;
        for (uint32_t b = 0; b < bw; b++) {
            new_beams[b].tokens = (uint32_t*)nimcp_calloc(max_len, sizeof(uint32_t));
            if (!new_beams[b].tokens) {
                for (uint32_t j = 0; j < b; j++) nimcp_free(new_beams[j].tokens);
                nimcp_free(new_beams);
                new_beams = NULL;
                break;
            }
        }
        if (!new_beams) break;

        for (uint32_t b = 0; b < bw; b++) {
            if (b < n_cand) {
                uint32_t src = cands[b].beam;
                memcpy(new_beams[b].tokens, beams[src].tokens,
                       beams[src].length * sizeof(uint32_t));
                new_beams[b].length = beams[src].length;
                new_beams[b].tokens[new_beams[b].length++] = cands[b].token;
                new_beams[b].log_prob = cands[b].score;
                new_beams[b].finished = (cands[b].token == gen->config.eos_id);
            } else {
                new_beams[b].finished = true;
            }
        }

        /* Swap */
        for (uint32_t b = 0; b < bw; b++) nimcp_free(beams[b].tokens);
        nimcp_free(beams);
        beams = new_beams;
    }

    /* Pick best beam */
    uint32_t best_b = 0;
    float best_score = -FLT_MAX;
    for (uint32_t b = 0; b < bw; b++) {
        /* Length-normalised score */
        float score = (beams[b].length > 0)
            ? beams[b].log_prob / (float)beams[b].length
            : -FLT_MAX;
        if (score > best_score) {
            best_score = score;
            best_b = b;
        }
    }

    /* Fill result */
    uint32_t n = beams[best_b].length;
    result->num_tokens = n;
    result->token_ids = (uint32_t*)nimcp_calloc(n > 0 ? n : 1, sizeof(uint32_t));
    result->token_confidences = (float*)nimcp_calloc(n > 0 ? n : 1, sizeof(float));
    if (result->token_ids && n > 0) {
        memcpy(result->token_ids, beams[best_b].tokens, n * sizeof(uint32_t));
    }

    /* Estimate per-token confidence from beam score */
    float avg_lp = (n > 0) ? beams[best_b].log_prob / (float)n : 0.0f;
    float conf = expf(avg_lp);
    if (conf > 1.0f) conf = 1.0f;
    for (uint32_t i = 0; i < n; i++) {
        if (result->token_confidences) result->token_confidences[i] = conf;
    }
    result->overall_confidence = conf;
    result->perplexity = (n > 0) ? expf(-beams[best_b].log_prob / (float)n) : 1.0f;

    /* Decode to text */
    result->text = (char*)nimcp_calloc(n * 32 + 64, sizeof(char));
    if (result->text && result->token_ids && gen->tokenizer) {
        tokenizer_decode(gen->tokenizer, result->token_ids, n,
                         result->text, n * 32 + 63);
    }

    /* Cleanup */
    nimcp_free(logits_buf); nimcp_free(hidden_buf);
    nimcp_free(embed_buf); nimcp_free(proj_buf);
    nimcp_free(cands);
    for (uint32_t b = 0; b < bw; b++) nimcp_free(beams[b].tokens);
    nimcp_free(beams);

    return 0;
}

/*=============================================================================
 * Static Helpers - Core Forward / Token Selection
 *===========================================================================*/

/**
 * WHAT: Ensure the cognitive projection matrix is allocated for the given dim
 * WHY:  We lazily allocate because cognitive_dim is unknown until first call
 * HOW:  Xavier-init a [cognitive_dim x hidden_dim] matrix
 */
static int ensure_cognitive_projection(language_generator_t* gen,
                                       uint32_t state_dim) {
    if (gen->cognitive_projection && gen->cognitive_dim == state_dim) return 0;

    /* Free previous if dimensions changed */
    if (gen->cognitive_projection) {
        nimcp_free(gen->cognitive_projection);
        gen->cognitive_projection = NULL;
    }

    uint32_t hdim = gen->config.hidden_dim;
    gen->cognitive_projection = (float*)nimcp_malloc(
        (size_t)state_dim * hdim * sizeof(float));
    if (!gen->cognitive_projection) {
        LOG_ERROR("language_generator: failed to allocate cognitive projection "
                  "[%u x %u]", state_dim, hdim);
        return -1;
    }
    xavier_init(gen->cognitive_projection, state_dim, hdim, &gen->rng_seed);
    gen->cognitive_dim = state_dim;
    LOG_INFO("language_generator: allocated cognitive projection [%u x %u]",
             state_dim, hdim);
    return 0;
}

/**
 * WHAT: Select the next token from logits according to current strategy
 * WHY:  Centralises all decoding strategy logic in one place
 * HOW:  Applies temperature, repetition penalty, softmax, then strategy filter
 *
 * @param logits     Mutable logits buffer [vocab_size]. Modified in place.
 * @param vocab_size Size of vocabulary
 * @param gen        Generator (for config and recent-token state)
 * @param out_prob   Output: probability of selected token
 * @return Selected token ID
 */
static uint32_t select_next_token(float* logits, uint32_t vocab_size,
                                  language_generator_t* gen,
                                  float* out_prob) {
    /* Apply temperature scaling */
    if (gen->config.temperature > 0.0f && gen->config.temperature != 1.0f) {
        float inv_t = 1.0f / gen->config.temperature;
        for (uint32_t i = 0; i < vocab_size; i++) {
            logits[i] *= inv_t;
        }
    }

    /* Apply repetition penalty */
    apply_repetition_penalty(logits, vocab_size,
                             gen->recent_tokens, gen->recent_count,
                             gen->config.repetition_penalty);

    /* Softmax to get probabilities */
    softmax_inplace(logits, vocab_size);

    uint32_t token = 0;

    switch (gen->config.strategy) {
    case GENERATION_GREEDY:
        token = argmax(logits, vocab_size);
        break;

    case GENERATION_SAMPLING:
        token = sample_categorical(logits, vocab_size, &gen->rng_seed);
        break;

    case GENERATION_TOP_K:
        apply_top_k(logits, vocab_size, gen->config.top_k);
        token = sample_categorical(logits, vocab_size, &gen->rng_seed);
        break;

    case GENERATION_TOP_P:
        apply_top_p(logits, vocab_size, gen->config.top_p);
        token = sample_categorical(logits, vocab_size, &gen->rng_seed);
        break;

    case GENERATION_BEAM_SEARCH:
        /* Beam search is handled separately at a higher level.
         * If we reach here fallback to greedy. */
        token = argmax(logits, vocab_size);
        break;
    }

    if (out_prob) *out_prob = logits[token];
    return token;
}

/*=============================================================================
 * Public API - Config
 *===========================================================================*/

generator_config_t generator_default_config(void) {
    generator_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.max_sequence_length = LANGGEN_DEFAULT_MAX_SEQ_LEN;
    cfg.hidden_dim          = LANGGEN_DEFAULT_HIDDEN_DIM;
    cfg.num_lnn_neurons     = LANGGEN_DEFAULT_NUM_NEURONS;
    cfg.temperature         = LANGGEN_DEFAULT_TEMPERATURE;
    cfg.top_p               = LANGGEN_DEFAULT_TOP_P;
    cfg.top_k               = LANGGEN_DEFAULT_TOP_K;
    cfg.strategy            = GENERATION_TOP_P;
    cfg.beam_width          = LANGGEN_DEFAULT_BEAM_WIDTH;
    cfg.repetition_penalty  = LANGGEN_DEFAULT_REP_PENALTY;
    cfg.learning_rate       = LANGGEN_DEFAULT_LEARNING_RATE;
    cfg.eos_id              = LANGGEN_DEFAULT_EOS_ID;
    return cfg;
}

/*=============================================================================
 * Public API - Lifecycle
 *===========================================================================*/

language_generator_t* language_generator_create(
    const generator_config_t* config,
    void* tokenizer,
    void* embedding,
    uint32_t vocab_size,
    uint32_t embed_dim)
{
    /* Guard: vocab must be non-zero */
    if (vocab_size == 0) {
        LOG_ERROR("language_generator_create: vocab_size must be > 0");
        return NULL;
    }
    if (embed_dim == 0) {
        LOG_ERROR("language_generator_create: embed_dim must be > 0");
        return NULL;
    }

    language_generator_t* gen = (language_generator_t*)nimcp_calloc(
        1, sizeof(language_generator_t));
    if (!gen) {
        LOG_ERROR("language_generator_create: allocation failed");
        return NULL;
    }

    /* Copy config or use defaults */
    if (config) {
        gen->config = *config;
    } else {
        gen->config = generator_default_config();
    }

    /* Clamp safety on config */
    if (gen->config.hidden_dim == 0)          gen->config.hidden_dim = LANGGEN_DEFAULT_HIDDEN_DIM;
    if (gen->config.num_lnn_neurons == 0)     gen->config.num_lnn_neurons = LANGGEN_DEFAULT_NUM_NEURONS;
    if (gen->config.max_sequence_length == 0) gen->config.max_sequence_length = LANGGEN_DEFAULT_MAX_SEQ_LEN;
    if (gen->config.temperature <= 0.0f)      gen->config.temperature = LANGGEN_DEFAULT_TEMPERATURE;
    if (gen->config.repetition_penalty < 1.0f) gen->config.repetition_penalty = 1.0f;
    if (gen->config.beam_width == 0)          gen->config.beam_width = LANGGEN_DEFAULT_BEAM_WIDTH;
    if (gen->config.learning_rate <= 0.0f)    gen->config.learning_rate = LANGGEN_DEFAULT_LEARNING_RATE;

    gen->tokenizer  = tokenizer;
    gen->embedding  = embedding;
    gen->vocab_size = vocab_size;
    gen->embed_dim  = embed_dim;

    /* Seed per-instance PRNG from time + pointer for thread safety */
    gen->rng_seed = (unsigned int)time(NULL) ^ (unsigned int)(uintptr_t)gen;
    if (gen->rng_seed == 0) gen->rng_seed = 1;

    uint32_t hdim = gen->config.hidden_dim;
    uint32_t lnn_n = gen->config.num_lnn_neurons;

    /* --- Create LNN decoder (NCP architecture) --- */
    gen->decoder_lnn = lnn_network_create_ncp(
        hdim,   /* n_inputs: projected cognitive/embedding vector */
        lnn_n,  /* n_inter:  interneurons */
        lnn_n,  /* n_command: command neurons */
        hdim    /* n_outputs: hidden state for projection */
    );
    if (!gen->decoder_lnn) {
        LOG_ERROR("language_generator_create: LNN decoder creation failed");
        language_generator_destroy(gen);
        return NULL;
    }

    /* Initialise LNN weights with a time-based seed */
    lnn_network_init_weights(gen->decoder_lnn, (uint64_t)time(NULL));

    /* --- Output projection [hidden_dim x vocab_size] --- */
    gen->output_projection = (float*)nimcp_malloc(
        (size_t)hdim * vocab_size * sizeof(float));
    if (!gen->output_projection) {
        LOG_ERROR("language_generator_create: output projection alloc failed");
        language_generator_destroy(gen);
        return NULL;
    }
    xavier_init(gen->output_projection, hdim, vocab_size, &gen->rng_seed);

    /* --- Output bias [vocab_size] zeroed --- */
    gen->output_bias = (float*)nimcp_calloc(vocab_size, sizeof(float));
    if (!gen->output_bias) {
        LOG_ERROR("language_generator_create: output bias alloc failed");
        language_generator_destroy(gen);
        return NULL;
    }

    /* --- Embedding -> hidden projection [embed_dim x hidden_dim] --- */
    gen->embed_projection = (float*)nimcp_malloc(
        (size_t)embed_dim * hdim * sizeof(float));
    if (!gen->embed_projection) {
        LOG_ERROR("language_generator_create: embed projection alloc failed");
        language_generator_destroy(gen);
        return NULL;
    }
    /* NOTE: output of matvec(embed_projection, embed, ...) is [hidden_dim],
     * so the matrix shape is [hidden_dim x embed_dim] (rows = hdim). */
    xavier_init(gen->embed_projection, embed_dim, hdim, &gen->rng_seed);

    /* --- Cognitive projection (lazy — allocated on first generate call) --- */
    gen->cognitive_projection = NULL;
    gen->cognitive_dim = 0;

    /* --- Repetition tracking --- */
    gen->recent_capacity = LANGGEN_RECENT_CAPACITY;
    gen->recent_tokens = (uint32_t*)nimcp_calloc(
        gen->recent_capacity, sizeof(uint32_t));
    if (!gen->recent_tokens) {
        LOG_ERROR("language_generator_create: recent token buffer alloc failed");
        language_generator_destroy(gen);
        return NULL;
    }
    gen->recent_count = 0;

    /* --- Statistics --- */
    gen->total_generations = 0;
    gen->total_tokens_generated = 0;
    gen->cumulative_perplexity = 0.0f;

    LOG_INFO("language_generator_create: created (vocab=%u, hdim=%u, "
             "lnn_n=%u, embed=%u, strategy=%d)",
             vocab_size, hdim, lnn_n, embed_dim, (int)gen->config.strategy);
    return gen;
}

void language_generator_destroy(language_generator_t* gen) {
    if (!gen) return;

    if (gen->decoder_lnn) {
        lnn_network_destroy(gen->decoder_lnn);
        gen->decoder_lnn = NULL;
    }

    nimcp_free(gen->output_projection);
    nimcp_free(gen->output_bias);
    nimcp_free(gen->embed_projection);
    nimcp_free(gen->cognitive_projection);
    nimcp_free(gen->recent_tokens);

    gen->output_projection = NULL;
    gen->output_bias = NULL;
    gen->embed_projection = NULL;
    gen->cognitive_projection = NULL;
    gen->recent_tokens = NULL;

    nimcp_free(gen);
}

/*=============================================================================
 * Public API - Core Generation
 *===========================================================================*/

int language_generator_generate(
    language_generator_t* gen,
    const float* cognitive_state,
    uint32_t state_dim,
    generation_result_t* result)
{
    /* Guard clauses */
    if (!gen)              { return -1; }
    if (!cognitive_state)  { LOG_ERROR("language_generator_generate: NULL state"); return -1; }
    if (state_dim == 0)    { LOG_ERROR("language_generator_generate: state_dim=0"); return -1; }
    if (!result)           { LOG_ERROR("language_generator_generate: NULL result"); return -1; }

    memset(result, 0, sizeof(*result));

    uint32_t hdim  = gen->config.hidden_dim;
    uint32_t vocab = gen->vocab_size;

    /* Ensure cognitive projection exists for this state_dim */
    if (ensure_cognitive_projection(gen, state_dim) != 0) return -1;

    /* Project cognitive state to hidden dimension:
     * hidden_input[hdim] = cognitive_projection[hdim x state_dim] @ state[state_dim]
     * (Note: matrix stored as [state_dim * hdim] but we treat it as [hdim rows x state_dim cols]) */
    float* hidden_input = (float*)nimcp_malloc(hdim * sizeof(float));
    if (!hidden_input) return -1;
    matvec(gen->cognitive_projection, cognitive_state, hidden_input, hdim, state_dim);

    /* For beam search, delegate to specialised function */
    if (gen->config.strategy == GENERATION_BEAM_SEARCH) {
        int rc = beam_search_generate(gen, hidden_input, result);
        nimcp_free(hidden_input);
        if (rc == 0) {
            gen->total_generations++;
            gen->total_tokens_generated += result->num_tokens;
            gen->cumulative_perplexity += result->perplexity;
        }
        return rc;
    }

    /* Reset LNN state for fresh generation */
    lnn_network_reset_state(gen->decoder_lnn);
    clear_recent(gen);

    /* Pre-allocate buffers for the generation loop */
    uint32_t max_len = gen->config.max_sequence_length;
    uint32_t* token_ids = (uint32_t*)nimcp_calloc(max_len, sizeof(uint32_t));
    float*    confs     = (float*)nimcp_calloc(max_len, sizeof(float));
    float*    logits    = (float*)nimcp_malloc(vocab * sizeof(float));
    float*    lnn_out   = (float*)nimcp_malloc(hdim * sizeof(float));
    float*    embed_buf = (float*)nimcp_malloc(gen->embed_dim * sizeof(float));
    float*    proj_buf  = (float*)nimcp_malloc(hdim * sizeof(float));

    if (!token_ids || !confs || !logits || !lnn_out || !embed_buf || !proj_buf) {
        nimcp_free(hidden_input); nimcp_free(token_ids); nimcp_free(confs);
        nimcp_free(logits); nimcp_free(lnn_out); nimcp_free(embed_buf);
        nimcp_free(proj_buf);
        LOG_ERROR("language_generator_generate: buffer allocation failed");
        return -1;
    }

    /* Create LNN input/output tensors */
    uint32_t tdims[1] = {hdim};
    nimcp_tensor_t* lnn_input  = nimcp_tensor_create(tdims, 1, NIMCP_DTYPE_F32);
    nimcp_tensor_t* lnn_output = nimcp_tensor_create(tdims, 1, NIMCP_DTYPE_F32);
    if (!lnn_input || !lnn_output) {
        nimcp_tensor_destroy(lnn_input);
        nimcp_tensor_destroy(lnn_output);
        nimcp_free(hidden_input); nimcp_free(token_ids); nimcp_free(confs);
        nimcp_free(logits); nimcp_free(lnn_out); nimcp_free(embed_buf);
        nimcp_free(proj_buf);
        return -1;
    }

    /* Seed the LNN with the projected cognitive state */
    memcpy(nimcp_tensor_data(lnn_input), hidden_input, hdim * sizeof(float));
    nimcp_free(hidden_input);
    hidden_input = NULL;

    /* ---- Autoregressive generation loop ---- */
    uint32_t num_generated = 0;
    float log_prob_sum = 0.0f;

    for (uint32_t step = 0; step < max_len; step++) {
        /* (a) Forward through LNN decoder */
        int rc = lnn_network_forward_step(gen->decoder_lnn, lnn_input,
                                          lnn_output, 1.0f);
        if (rc != 0) {
            LOG_WARN("language_generator: LNN forward step failed at step %u", step);
            break;
        }

        /* (b) Copy LNN output hidden state */
        memcpy(lnn_out, nimcp_tensor_data_const(lnn_output), hdim * sizeof(float));

        /* (c) Compute logits = output_projection @ hidden + bias
         * output_projection is [vocab x hdim] row-major */
        matvec(gen->output_projection, lnn_out, logits, vocab, hdim);
        add_bias(logits, gen->output_bias, vocab);

        /* (d-g) Select next token (applies temperature, rep penalty, strategy) */
        float prob = 0.0f;
        uint32_t tok = select_next_token(logits, vocab, gen, &prob);

        /* (h) Store */
        token_ids[num_generated] = tok;
        confs[num_generated] = prob;
        num_generated++;

        /* Accumulate log prob for perplexity */
        if (prob > 0.0f) {
            log_prob_sum += logf(prob);
        } else {
            log_prob_sum += -20.0f; /* ~exp(-20) ≈ 0 */
        }

        /* (i) Check for EOS */
        if (tok == gen->config.eos_id) break;

        /* (j) Embed the generated token and project to hidden_dim for next input */
        if (gen->embedding) {
            embedding_lookup(gen->embedding, tok, embed_buf);
            matvec(gen->embed_projection, embed_buf, proj_buf, hdim, gen->embed_dim);
            memcpy(nimcp_tensor_data(lnn_input), proj_buf, hdim * sizeof(float));
        } else {
            /* No embedding available: feed the LNN output back as input */
            memcpy(nimcp_tensor_data(lnn_input), lnn_out, hdim * sizeof(float));
        }

        /* (k) Update repetition tracker */
        push_recent(gen, tok);
    }

    /* ---- Compute perplexity ---- */
    float perplexity = 1.0f;
    if (num_generated > 0) {
        perplexity = expf(-log_prob_sum / (float)num_generated);
    }

    /* ---- Compute overall confidence ---- */
    float conf_sum = 0.0f;
    for (uint32_t i = 0; i < num_generated; i++) conf_sum += confs[i];
    float overall_conf = (num_generated > 0) ? conf_sum / (float)num_generated : 0.0f;

    /* ---- Decode tokens to text ---- */
    char* text = NULL;
    if (gen->tokenizer && num_generated > 0) {
        uint32_t text_cap = num_generated * 32 + 64;
        text = (char*)nimcp_calloc(text_cap, sizeof(char));
        if (text) {
            tokenizer_decode(gen->tokenizer, token_ids, num_generated,
                             text, text_cap - 1);
        }
    } else {
        text = nimcp_strdup("");
    }

    /* ---- Fill result ---- */
    result->text               = text;
    result->token_ids          = (uint32_t*)nimcp_malloc(num_generated * sizeof(uint32_t));
    result->token_confidences  = (float*)nimcp_malloc(num_generated * sizeof(float));
    result->num_tokens         = num_generated;
    result->overall_confidence = overall_conf;
    result->perplexity         = perplexity;

    if (result->token_ids && num_generated > 0)
        memcpy(result->token_ids, token_ids, num_generated * sizeof(uint32_t));
    if (result->token_confidences && num_generated > 0)
        memcpy(result->token_confidences, confs, num_generated * sizeof(float));

    /* Update statistics */
    gen->total_generations++;
    gen->total_tokens_generated += num_generated;
    gen->cumulative_perplexity  += perplexity;

    /* Cleanup working buffers */
    nimcp_tensor_destroy(lnn_input);
    nimcp_tensor_destroy(lnn_output);
    nimcp_free(token_ids);
    nimcp_free(confs);
    nimcp_free(logits);
    nimcp_free(lnn_out);
    nimcp_free(embed_buf);
    nimcp_free(proj_buf);

    return 0;
}

/*=============================================================================
 * Public API - Generate from Prompt
 *===========================================================================*/

int language_generator_generate_from_prompt(
    language_generator_t* gen,
    const char* prompt,
    generation_result_t* result)
{
    /* Guard clauses */
    if (!gen)    { return -1; }
    if (!prompt) { LOG_ERROR("language_generator_generate_from_prompt: NULL prompt"); return -1; }
    if (!result) { LOG_ERROR("language_generator_generate_from_prompt: NULL result"); return -1; }

    memset(result, 0, sizeof(*result));

    uint32_t hdim  = gen->config.hidden_dim;
    uint32_t vocab = gen->vocab_size;
    uint32_t max_prompt_tokens = 512;

    /* Encode prompt */
    uint32_t* prompt_ids = (uint32_t*)nimcp_calloc(max_prompt_tokens, sizeof(uint32_t));
    if (!prompt_ids) return -1;

    uint32_t prompt_len = 0;
    if (gen->tokenizer) {
        int rc = tokenizer_encode(gen->tokenizer, prompt, prompt_ids,
                                  max_prompt_tokens, &prompt_len);
        if (rc != 0 || prompt_len == 0) {
            LOG_WARN("language_generator: prompt encoding failed or empty");
            nimcp_free(prompt_ids);
            /* Fall back: generate from a zero cognitive state */
            float zero_state[1] = {0.0f};
            return language_generator_generate(gen, zero_state, 1, result);
        }
    } else {
        nimcp_free(prompt_ids);
        LOG_ERROR("language_generator: no tokenizer for prompt encoding");
        return -1;
    }

    /* Reset LNN state */
    lnn_network_reset_state(gen->decoder_lnn);
    clear_recent(gen);

    /* Working buffers */
    float* embed_buf = (float*)nimcp_malloc(gen->embed_dim * sizeof(float));
    float* proj_buf  = (float*)nimcp_malloc(hdim * sizeof(float));
    if (!embed_buf || !proj_buf) {
        nimcp_free(prompt_ids); nimcp_free(embed_buf); nimcp_free(proj_buf);
        return -1;
    }

    uint32_t tdims[1] = {hdim};
    nimcp_tensor_t* lnn_input  = nimcp_tensor_create(tdims, 1, NIMCP_DTYPE_F32);
    nimcp_tensor_t* lnn_output = nimcp_tensor_create(tdims, 1, NIMCP_DTYPE_F32);
    if (!lnn_input || !lnn_output) {
        nimcp_tensor_destroy(lnn_input);
        nimcp_tensor_destroy(lnn_output);
        nimcp_free(prompt_ids); nimcp_free(embed_buf); nimcp_free(proj_buf);
        return -1;
    }

    /* Teacher-force through the prompt tokens: embed each token, project, LNN forward */
    for (uint32_t t = 0; t < prompt_len; t++) {
        if (gen->embedding) {
            embedding_lookup(gen->embedding, prompt_ids[t],
                             embed_buf);
            matvec(gen->embed_projection, embed_buf, proj_buf,
                   hdim, gen->embed_dim);
            memcpy(nimcp_tensor_data(lnn_input), proj_buf, hdim * sizeof(float));
        } else {
            /* No embedding: zero input */
            memset(nimcp_tensor_data(lnn_input), 0, hdim * sizeof(float));
        }
        lnn_network_forward_step(gen->decoder_lnn, lnn_input, lnn_output, 1.0f);
        push_recent(gen, prompt_ids[t]);
    }

    nimcp_free(prompt_ids);

    /* Now switch to autoregressive generation using the LNN state */
    uint32_t max_gen = gen->config.max_sequence_length;
    uint32_t* gen_ids = (uint32_t*)nimcp_calloc(max_gen, sizeof(uint32_t));
    float*    confs   = (float*)nimcp_calloc(max_gen, sizeof(float));
    float*    logits  = (float*)nimcp_malloc(vocab * sizeof(float));
    float*    lnn_out = (float*)nimcp_malloc(hdim * sizeof(float));

    if (!gen_ids || !confs || !logits || !lnn_out) {
        nimcp_tensor_destroy(lnn_input); nimcp_tensor_destroy(lnn_output);
        nimcp_free(embed_buf); nimcp_free(proj_buf);
        nimcp_free(gen_ids); nimcp_free(confs); nimcp_free(logits); nimcp_free(lnn_out);
        return -1;
    }

    uint32_t num_gen = 0;
    float log_prob_sum = 0.0f;

    /* The last LNN output from prompt processing is already in lnn_output.
     * Use that as the starting hidden. */
    memcpy(lnn_out, nimcp_tensor_data_const(lnn_output), hdim * sizeof(float));

    for (uint32_t step = 0; step < max_gen; step++) {
        /* Compute logits from current hidden state */
        matvec(gen->output_projection, lnn_out, logits, vocab, hdim);
        add_bias(logits, gen->output_bias, vocab);

        /* Select next token */
        float prob = 0.0f;
        uint32_t tok = select_next_token(logits, vocab, gen, &prob);

        gen_ids[num_gen] = tok;
        confs[num_gen]   = prob;
        num_gen++;

        if (prob > 0.0f) log_prob_sum += logf(prob);
        else              log_prob_sum += -20.0f;

        if (tok == gen->config.eos_id) break;

        /* Embed and project for next step */
        if (gen->embedding) {
            embedding_lookup(gen->embedding, tok, embed_buf);
            matvec(gen->embed_projection, embed_buf, proj_buf, hdim, gen->embed_dim);
            memcpy(nimcp_tensor_data(lnn_input), proj_buf, hdim * sizeof(float));
        } else {
            memcpy(nimcp_tensor_data(lnn_input), lnn_out, hdim * sizeof(float));
        }

        lnn_network_forward_step(gen->decoder_lnn, lnn_input, lnn_output, 1.0f);
        memcpy(lnn_out, nimcp_tensor_data_const(lnn_output), hdim * sizeof(float));
        push_recent(gen, tok);
    }

    /* Fill result */
    float perplexity = (num_gen > 0) ? expf(-log_prob_sum / (float)num_gen) : 1.0f;
    float conf_sum = 0.0f;
    for (uint32_t i = 0; i < num_gen; i++) conf_sum += confs[i];
    float overall = (num_gen > 0) ? conf_sum / (float)num_gen : 0.0f;

    result->num_tokens         = num_gen;
    result->overall_confidence = overall;
    result->perplexity         = perplexity;

    result->token_ids = (uint32_t*)nimcp_malloc(
        (num_gen > 0 ? num_gen : 1) * sizeof(uint32_t));
    result->token_confidences = (float*)nimcp_malloc(
        (num_gen > 0 ? num_gen : 1) * sizeof(float));
    if (result->token_ids && num_gen > 0)
        memcpy(result->token_ids, gen_ids, num_gen * sizeof(uint32_t));
    if (result->token_confidences && num_gen > 0)
        memcpy(result->token_confidences, confs, num_gen * sizeof(float));

    /* Decode to text */
    if (gen->tokenizer && num_gen > 0) {
        uint32_t cap = num_gen * 32 + 64;
        result->text = (char*)nimcp_calloc(cap, sizeof(char));
        if (result->text) {
            tokenizer_decode(gen->tokenizer, gen_ids, num_gen,
                             result->text, cap - 1);
        }
    } else {
        result->text = nimcp_strdup("");
    }

    /* Update stats */
    gen->total_generations++;
    gen->total_tokens_generated += num_gen;
    gen->cumulative_perplexity  += perplexity;

    /* Cleanup */
    nimcp_tensor_destroy(lnn_input);
    nimcp_tensor_destroy(lnn_output);
    nimcp_free(embed_buf);
    nimcp_free(proj_buf);
    nimcp_free(gen_ids);
    nimcp_free(confs);
    nimcp_free(logits);
    nimcp_free(lnn_out);

    return 0;
}

/*=============================================================================
 * Public API - Generate from Reasoning Chain
 *===========================================================================*/

int language_generator_generate_from_reasoning(
    language_generator_t* gen,
    const void* chain,
    generation_result_t* result)
{
    /* Guard clauses */
    if (!gen)    { return -1; }
    if (!chain)  { LOG_ERROR("language_generator_generate_from_reasoning: NULL chain"); return -1; }
    if (!result) { LOG_ERROR("language_generator_generate_from_reasoning: NULL result"); return -1; }

    /* Extract features from meta_reasoning_chain_t:
     *   - overall_confidence (float)
     *   - num_steps (uint32_t)
     *   - progress_rate (float)
     *   - completed (bool)
     *   - step descriptions hashed to feature vector
     *
     * We cast the opaque chain pointer to access its fields.
     * The struct layout is defined in nimcp_meta_reasoning.h:
     *   typedef struct {
     *       meta_reasoning_step_t* steps;
     *       uint32_t num_steps;
     *       float overall_confidence;
     *       float progress_rate;
     *       bool completed;
     *   } meta_reasoning_chain_t;
     */

    /* Access chain fields via byte offsets to avoid header dependency.
     * We construct a feature vector of fixed size. */
    const uint32_t feature_dim = 64;
    float* features = (float*)nimcp_calloc(feature_dim, sizeof(float));
    if (!features) return -1;

    /* Read fields from the chain struct.
     * Layout (from nimcp_meta_reasoning.h):
     *   offset 0:  meta_reasoning_step_t* steps (pointer)
     *   offset 8:  uint32_t num_steps
     *   offset 12: float overall_confidence
     *   offset 16: float progress_rate
     *   offset 20: bool completed
     */
    const uint8_t* raw = (const uint8_t*)chain;

    uint32_t num_steps = 0;
    memcpy(&num_steps, raw + sizeof(void*), sizeof(uint32_t));

    float overall_conf = 0.0f;
    memcpy(&overall_conf, raw + sizeof(void*) + sizeof(uint32_t), sizeof(float));

    float progress_rate = 0.0f;
    memcpy(&progress_rate, raw + sizeof(void*) + sizeof(uint32_t) + sizeof(float),
           sizeof(float));

    bool completed = false;
    memcpy(&completed, raw + sizeof(void*) + 2 * sizeof(uint32_t) + 2 * sizeof(float),
           sizeof(bool));

    /* Feature 0-3: direct scalar features */
    features[0] = overall_conf;
    features[1] = progress_rate;
    features[2] = (float)num_steps / 64.0f;   /* Normalise to [0, 1] range */
    features[3] = completed ? 1.0f : 0.0f;

    /* Features 4+: hash-based features from the steps pointer.
     * We use the chain pointer itself as a hash seed to create
     * a pseudo-random but deterministic feature vector. */
    uint32_t seed = hash_string((const char*)&chain);
    for (uint32_t i = 4; i < feature_dim; i++) {
        seed = seed * 1103515245 + 12345;
        features[i] = ((float)(seed >> 16) / 32768.0f) - 1.0f;
        /* Modulate by confidence and progress */
        features[i] *= (overall_conf * 0.5f + 0.5f);
    }

    /* Delegate to the core generate function */
    int rc = language_generator_generate(gen, features, feature_dim, result);
    nimcp_free(features);
    return rc;
}

/*=============================================================================
 * Public API - Result Cleanup
 *===========================================================================*/

void generation_result_cleanup(generation_result_t* result) {
    if (!result) return;

    nimcp_free(result->text);
    nimcp_free(result->token_ids);
    nimcp_free(result->token_confidences);

    memset(result, 0, sizeof(*result));
}

/*=============================================================================
 * Public API - Training
 *===========================================================================*/

int language_generator_train_step(
    language_generator_t* gen,
    const uint32_t* input_ids,
    const uint32_t* target_ids,
    uint32_t seq_len,
    float* loss)
{
    /* Guard clauses */
    if (!gen)        { return -1; }
    if (!input_ids)  { LOG_ERROR("language_generator_train_step: NULL input_ids"); return -1; }
    if (!target_ids) { LOG_ERROR("language_generator_train_step: NULL target_ids"); return -1; }
    if (seq_len == 0){ LOG_ERROR("language_generator_train_step: seq_len=0"); return -1; }
    if (!loss)       { LOG_ERROR("language_generator_train_step: NULL loss"); return -1; }

    uint32_t hdim  = gen->config.hidden_dim;
    uint32_t vocab = gen->vocab_size;
    float lr       = gen->config.learning_rate;

    /* Enable LNN training mode (records state for potential BPTT) */
    lnn_network_set_training(gen->decoder_lnn, true);
    lnn_network_reset_state(gen->decoder_lnn);

    /* Working buffers */
    float* embed_buf   = (float*)nimcp_malloc(gen->embed_dim * sizeof(float));
    float* proj_buf    = (float*)nimcp_malloc(hdim * sizeof(float));
    float* lnn_out     = (float*)nimcp_malloc(hdim * sizeof(float));
    float* logits      = (float*)nimcp_malloc(vocab * sizeof(float));
    float* probs       = (float*)nimcp_malloc(vocab * sizeof(float));
    float* d_logits    = (float*)nimcp_malloc(vocab * sizeof(float));
    float* d_proj_W    = (float*)nimcp_calloc((size_t)vocab * hdim, sizeof(float));
    float* d_proj_b    = (float*)nimcp_calloc(vocab, sizeof(float));
    float* d_embed_W   = (float*)nimcp_calloc((size_t)hdim * gen->embed_dim, sizeof(float));

    if (!embed_buf || !proj_buf || !lnn_out || !logits || !probs ||
        !d_logits || !d_proj_W || !d_proj_b || !d_embed_W) {
        nimcp_free(embed_buf); nimcp_free(proj_buf); nimcp_free(lnn_out);
        nimcp_free(logits); nimcp_free(probs); nimcp_free(d_logits);
        nimcp_free(d_proj_W); nimcp_free(d_proj_b); nimcp_free(d_embed_W);
        lnn_network_set_training(gen->decoder_lnn, false);
        return -1;
    }

    uint32_t tdims[1] = {hdim};
    nimcp_tensor_t* lnn_input  = nimcp_tensor_create(tdims, 1, NIMCP_DTYPE_F32);
    nimcp_tensor_t* lnn_output = nimcp_tensor_create(tdims, 1, NIMCP_DTYPE_F32);
    if (!lnn_input || !lnn_output) {
        nimcp_tensor_destroy(lnn_input); nimcp_tensor_destroy(lnn_output);
        nimcp_free(embed_buf); nimcp_free(proj_buf); nimcp_free(lnn_out);
        nimcp_free(logits); nimcp_free(probs); nimcp_free(d_logits);
        nimcp_free(d_proj_W); nimcp_free(d_proj_b); nimcp_free(d_embed_W);
        lnn_network_set_training(gen->decoder_lnn, false);
        return -1;
    }

    float total_loss = 0.0f;

    /* For each position in the sequence:
     * 1. Embed input token -> project to hidden_dim -> LNN forward
     * 2. Compute logits -> cross-entropy against target
     * 3. Accumulate gradients for output_projection and bias
     */
    for (uint32_t t = 0; t < seq_len; t++) {
        /* (1) Embed and project input token */
        if (gen->embedding) {
            embedding_lookup(gen->embedding, input_ids[t],
                             embed_buf);
            matvec(gen->embed_projection, embed_buf, proj_buf,
                   hdim, gen->embed_dim);
            memcpy(nimcp_tensor_data(lnn_input), proj_buf, hdim * sizeof(float));
        } else {
            memset(nimcp_tensor_data(lnn_input), 0, hdim * sizeof(float));
        }

        /* LNN forward step */
        lnn_network_forward_step(gen->decoder_lnn, lnn_input, lnn_output, 1.0f);
        memcpy(lnn_out, nimcp_tensor_data_const(lnn_output), hdim * sizeof(float));

        /* (2) Compute logits */
        matvec(gen->output_projection, lnn_out, logits, vocab, hdim);
        add_bias(logits, gen->output_bias, vocab);

        /* Softmax */
        memcpy(probs, logits, vocab * sizeof(float));
        softmax_inplace(probs, vocab);

        /* Cross-entropy loss for this position:
         * L = -log(probs[target])
         */
        uint32_t target = target_ids[t];
        if (target >= vocab) target = 0; /* safety clamp */
        float p_target = probs[target];
        if (p_target < 1e-10f) p_target = 1e-10f;
        total_loss += -logf(p_target);

        /* (3) Gradient of cross-entropy w.r.t. logits:
         * d_logits = probs - one_hot(target)
         */
        memcpy(d_logits, probs, vocab * sizeof(float));
        d_logits[target] -= 1.0f;

        /* Accumulate gradient for output_projection:
         * dL/dW[i][j] += d_logits[i] * lnn_out[j]
         * output_projection is [vocab x hdim]
         */
        for (uint32_t i = 0; i < vocab; i++) {
            float dl = d_logits[i];
            if (fabsf(dl) < 1e-12f) continue;
            float* dW_row = d_proj_W + (size_t)i * hdim;
            for (uint32_t j = 0; j < hdim; j++) {
                dW_row[j] += dl * lnn_out[j];
            }
        }

        /* Accumulate gradient for bias: dL/db[i] += d_logits[i] */
        for (uint32_t i = 0; i < vocab; i++) {
            d_proj_b[i] += d_logits[i];
        }

        /* Accumulate gradient for embed_projection (backprop through LNN output
         * is complex; we approximate by treating LNN as fixed and only updating
         * the embedding projection for this training step).
         *
         * dL/d(embed_proj)[r][c] += dL/d(proj_buf[r]) * embed_buf[c]
         * where dL/d(proj_buf) comes from backprop through LNN (approximated as
         * output_projection^T @ d_logits, then through the LNN identity assumption).
         *
         * For simplicity, we compute: d_hidden = output_projection^T @ d_logits
         * Then: dL/d(embed_proj)[r][c] += d_hidden[r] * embed_buf[c]
         */
        if (gen->embedding) {
            /* d_hidden[j] = sum_i(output_projection[i][j] * d_logits[i]) */
            float* d_hidden = proj_buf; /* reuse buffer */
            memset(d_hidden, 0, hdim * sizeof(float));
            for (uint32_t i = 0; i < vocab; i++) {
                float dl = d_logits[i];
                if (fabsf(dl) < 1e-12f) continue;
                const float* W_row = gen->output_projection + (size_t)i * hdim;
                for (uint32_t j = 0; j < hdim; j++) {
                    d_hidden[j] += dl * W_row[j];
                }
            }

            /* embed_projection is [hdim x embed_dim] */
            for (uint32_t r = 0; r < hdim; r++) {
                float dh = d_hidden[r];
                if (fabsf(dh) < 1e-12f) continue;
                float* dE_row = d_embed_W + (size_t)r * gen->embed_dim;
                for (uint32_t c = 0; c < gen->embed_dim; c++) {
                    dE_row[c] += dh * embed_buf[c];
                }
            }
        }
    }

    /* Average loss over sequence */
    *loss = total_loss / (float)seq_len;

    /* Apply SGD update: W -= lr * dW / seq_len */
    float inv_seq = 1.0f / (float)seq_len;

    /* Update output_projection */
    size_t proj_size = (size_t)vocab * hdim;
    for (size_t i = 0; i < proj_size; i++) {
        gen->output_projection[i] -= lr * d_proj_W[i] * inv_seq;
    }

    /* Update output_bias */
    for (uint32_t i = 0; i < vocab; i++) {
        gen->output_bias[i] -= lr * d_proj_b[i] * inv_seq;
    }

    /* Update embed_projection */
    size_t emb_size = (size_t)hdim * gen->embed_dim;
    for (size_t i = 0; i < emb_size; i++) {
        gen->embed_projection[i] -= lr * d_embed_W[i] * inv_seq;
    }

    /* Cleanup */
    lnn_network_set_training(gen->decoder_lnn, false);
    nimcp_tensor_destroy(lnn_input);
    nimcp_tensor_destroy(lnn_output);
    nimcp_free(embed_buf);
    nimcp_free(proj_buf);
    nimcp_free(lnn_out);
    nimcp_free(logits);
    nimcp_free(probs);
    nimcp_free(d_logits);
    nimcp_free(d_proj_W);
    nimcp_free(d_proj_b);
    nimcp_free(d_embed_W);

    return 0;
}

/*=============================================================================
 * Public API - Statistics / Control
 *===========================================================================*/

int language_generator_get_stats(
    const language_generator_t* gen,
    generator_stats_t* stats)
{
    if (!gen)   { return -1; }
    if (!stats) { return -1; }

    stats->total_generations      = gen->total_generations;
    stats->total_tokens_generated = gen->total_tokens_generated;

    stats->avg_perplexity = (gen->total_generations > 0)
        ? gen->cumulative_perplexity / (float)gen->total_generations
        : 0.0f;

    stats->avg_tokens_per_generation = (gen->total_generations > 0)
        ? (float)gen->total_tokens_generated / (float)gen->total_generations
        : 0.0f;

    return 0;
}

void language_generator_set_temperature(
    language_generator_t* gen,
    float temperature)
{
    if (!gen) return;
    if (temperature <= 0.0f) {
        LOG_WARN("language_generator_set_temperature: clamping %f to 0.01",
                 (double)temperature);
        temperature = 0.01f;
    }
    gen->config.temperature = temperature;
}

void language_generator_set_strategy(
    language_generator_t* gen,
    generation_strategy_t strategy)
{
    if (!gen) return;
    gen->config.strategy = strategy;
}
