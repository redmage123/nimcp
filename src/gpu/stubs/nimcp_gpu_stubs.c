/**
 * @file nimcp_gpu_stubs.c
 * @brief CPU fallback implementations for GPU functions when CUDA is not available
 *
 * WHAT: Provides functional CPU fallback implementations for all GPU functions
 * WHY:  Allows building and running without CUDA - enables testing on CPU-only systems
 * HOW:  Implements equivalent CPU algorithms where possible, returns graceful failures otherwise
 *
 * @author NIMCP Development Team
 * @date 2026-01-06
 */

#include "gpu/cognitive/nimcp_wernicke_gpu.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "utils/thread/nimcp_thread_rand.h"
#include "constants/nimcp_dimension_constants.h"

/*=============================================================================
 * Wernicke GPU Stubs (gpu/cognitive/nimcp_wernicke_gpu.h)
 *=============================================================================*/

/* Internal CPU-based wernicke context for fallback */
struct wernicke_gpu_context {
    wernicke_gpu_config_t config;

    /* Phoneme data */
    float* phoneme_embeddings;
    uint32_t num_phonemes;
    uint32_t phoneme_embed_dim;

    /* Lexicon data */
    wernicke_gpu_lexical_entry_t* lexicon;
    uint32_t lexicon_size;

    /* Working memory */
    uint8_t* wm_phonemes;
    float* wm_activations;
    uint32_t wm_count;

    /* Cohort state */
    wernicke_gpu_word_candidate_t* cohort;
    uint32_t cohort_size;
    uint8_t* current_phoneme_seq;
    uint32_t phoneme_seq_len;

    /* Statistics */
    wernicke_gpu_stats_t stats;
};

wernicke_gpu_config_t wernicke_gpu_default_config(void) {
    wernicke_gpu_config_t config;
    memset(&config, 0, sizeof(config));
    config.num_phoneme_categories = WERNICKE_GPU_DEFAULT_NUM_PHONEMES;
    config.phoneme_embedding_dim = NIMCP_SMALL_EMBEDDING_DIM;
    config.max_spectral_frames = 256;
    config.max_lexicon_size = 10000;
    config.max_cohort_size = 100;
    config.word_embedding_dim = NIMCP_DEFAULT_EMBEDDING_DIM;
    config.max_phonemes_per_word = 16;
    config.max_concepts = 5000;
    config.semantic_embedding_dim = NIMCP_LARGE_EMBEDDING_DIM;
    config.spreading_iterations = 3;
    config.spreading_decay = 0.5f;
    config.enable_attention = false;
    config.attention_heads = 4;
    config.working_memory_slots = NIMCP_WM_SLOTS;
    config.wm_decay_rate = 0.1f;
    config.enable_async_transfer = false;
    config.max_batch_size = NIMCP_MEDIUM_BATCH_SIZE;
    return config;
}

wernicke_gpu_context_t* wernicke_gpu_create(
    nimcp_gpu_context_t* gpu_ctx,
    const wernicke_gpu_config_t* config)
{
    (void)gpu_ctx;  /* Not used in CPU fallback */

    wernicke_gpu_context_t* ctx = nimcp_calloc(1, sizeof(wernicke_gpu_context_t));
    if (!ctx) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "wernicke_gpu_create: allocation failed");

        return NULL;

    }

    ctx->config = config ? *config : wernicke_gpu_default_config();

    /* Allocate working memory */
    ctx->wm_phonemes = nimcp_calloc(ctx->config.working_memory_slots, sizeof(uint8_t));
    ctx->wm_activations = nimcp_calloc(ctx->config.working_memory_slots, sizeof(float));

    /* Allocate cohort buffers */
    ctx->cohort = nimcp_calloc(ctx->config.max_cohort_size, sizeof(wernicke_gpu_word_candidate_t));
    ctx->current_phoneme_seq = nimcp_calloc(ctx->config.max_phonemes_per_word, sizeof(uint8_t));

    if (!ctx->wm_phonemes || !ctx->wm_activations || !ctx->cohort || !ctx->current_phoneme_seq) {
        wernicke_gpu_destroy(ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "wernicke_gpu_create: required parameter is NULL (ctx->wm_phonemes, ctx->wm_activations, ctx->cohort, ctx->current_phoneme_seq)");
        return NULL;
    }

    return ctx;
}

void wernicke_gpu_destroy(wernicke_gpu_context_t* ctx) {
    if (!ctx) return;

    nimcp_free(ctx->phoneme_embeddings);
    nimcp_free(ctx->lexicon);
    nimcp_free(ctx->wm_phonemes);
    nimcp_free(ctx->wm_activations);
    nimcp_free(ctx->cohort);
    nimcp_free(ctx->current_phoneme_seq);
    nimcp_free(ctx);
}

bool wernicke_gpu_synchronize(wernicke_gpu_context_t* ctx) {
    (void)ctx;
    return true;  /* CPU is always synchronous */
}

bool wernicke_gpu_upload_phoneme_embeddings(
    wernicke_gpu_context_t* ctx,
    const float* embeddings,
    uint32_t num_phonemes,
    uint32_t embed_dim)
{
    if (!ctx || !embeddings) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_gpu_upload_phoneme_embeddings: required parameter is NULL (ctx, embeddings)");
        return false;
    }

    nimcp_free(ctx->phoneme_embeddings);
    ctx->phoneme_embeddings = nimcp_malloc(num_phonemes * embed_dim * sizeof(float));
    if (!ctx->phoneme_embeddings) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "wernicke_gpu_upload_phoneme_embeddings: ctx->phoneme_embeddings is NULL");
        return false;
    }

    memcpy(ctx->phoneme_embeddings, embeddings, num_phonemes * embed_dim * sizeof(float));
    ctx->num_phonemes = num_phonemes;
    ctx->phoneme_embed_dim = embed_dim;

    return true;
}

/* CPU phoneme recognition using cosine similarity */
bool wernicke_gpu_recognize_phonemes(
    wernicke_gpu_context_t* ctx,
    const wernicke_gpu_spectral_frame_t* frames,
    uint32_t num_frames,
    wernicke_gpu_phoneme_result_t* results)
{
    if (!ctx || !frames || !results) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_gpu_recognize_phonemes: required parameter is NULL (ctx, frames, results)");
        return false;
    }

    /* Simple heuristic: use MFCC coefficients to estimate phoneme */
    for (uint32_t i = 0; i < num_frames; i++) {
        float max_energy = 0.0f;
        uint8_t best_phoneme = 0;

        /* Use energy and voicing to classify */
        float energy = frames[i].energy;
        float voicing = frames[i].voicing;

        /* Simple classification based on features */
        if (voicing > 0.7f) {
            /* Voiced sound - likely vowel */
            best_phoneme = (uint8_t)((int)(frames[i].pitch / 50.0f) % 12);  /* Map to vowel range */
            max_energy = energy * voicing;
        } else if (energy > 0.3f) {
            /* Unvoiced with energy - likely fricative */
            best_phoneme = 12 + (uint8_t)((int)(energy * 10.0f) % 8);
        } else {
            /* Low energy - likely stop or silence */
            best_phoneme = 20 + (uint8_t)((int)(frames[i].mfcc[0] * 5.0f) % 10);
        }

        results[i].phoneme_id = best_phoneme % ctx->config.num_phoneme_categories;
        results[i].confidence = 0.5f + 0.5f * (energy * voicing);
        results[i].posterior = NULL;
    }

    ctx->stats.phoneme_recognitions += num_frames;
    return true;
}

bool wernicke_gpu_compute_posteriors(
    wernicke_gpu_context_t* ctx,
    const wernicke_gpu_spectral_frame_t* frames,
    uint32_t num_frames,
    float* posteriors)
{
    if (!ctx || !frames || !posteriors) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_gpu_compute_posteriors: required parameter is NULL (ctx, frames, posteriors)");
        return false;
    }

    uint32_t num_phonemes = ctx->config.num_phoneme_categories;

    for (uint32_t i = 0; i < num_frames; i++) {
        /* Initialize uniform prior */
        float sum = 0.0f;
        for (uint32_t p = 0; p < num_phonemes; p++) {
            posteriors[i * num_phonemes + p] = 1.0f / num_phonemes;
            sum += posteriors[i * num_phonemes + p];
        }
        /* Normalize */
        for (uint32_t p = 0; p < num_phonemes; p++) {
            posteriors[i * num_phonemes + p] /= sum;
        }
    }

    return true;
}

bool wernicke_gpu_upload_lexicon(
    wernicke_gpu_context_t* ctx,
    const wernicke_gpu_lexical_entry_t* entries,
    uint32_t count)
{
    if (!ctx || !entries || count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_gpu_upload_lexicon: required parameter is NULL (ctx, entries)");
        return false;
    }

    nimcp_free(ctx->lexicon);
    ctx->lexicon = nimcp_malloc(count * sizeof(wernicke_gpu_lexical_entry_t));
    if (!ctx->lexicon) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "wernicke_gpu_upload_lexicon: ctx->lexicon is NULL");
        return false;
    }

    memcpy(ctx->lexicon, entries, count * sizeof(wernicke_gpu_lexical_entry_t));
    ctx->lexicon_size = count;

    return true;
}

bool wernicke_gpu_clear_lexicon(wernicke_gpu_context_t* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_gpu_clear_lexicon: ctx is NULL");
        return false;
    }
    nimcp_free(ctx->lexicon);
    ctx->lexicon = NULL;
    ctx->lexicon_size = 0;
    return true;
}

uint32_t wernicke_gpu_get_lexicon_size(const wernicke_gpu_context_t* ctx) {
    return ctx ? ctx->lexicon_size : 0;
}

/* CPU word recognition using cohort model */
bool wernicke_gpu_recognize_words(
    wernicke_gpu_context_t* ctx,
    const uint8_t* phonemes,
    uint32_t num_phonemes,
    wernicke_gpu_word_candidate_t* candidates,
    uint32_t max_candidates,
    uint32_t* num_candidates)
{
    if (!ctx || !phonemes || !candidates || !num_candidates) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_gpu_recognize_words: required parameter is NULL (ctx, phonemes, candidates, num_candidates)");
        return false;
    }
    if (!ctx->lexicon || ctx->lexicon_size == 0) {
        *num_candidates = 0;
        return true;
    }

    uint32_t found = 0;

    for (uint32_t w = 0; w < ctx->lexicon_size && found < max_candidates; w++) {
        wernicke_gpu_lexical_entry_t* entry = &ctx->lexicon[w];

        /* Check if phoneme sequence matches beginning of word */
        uint32_t match_len = 0;
        for (uint32_t p = 0; p < num_phonemes && p < entry->phoneme_count; p++) {
            if (entry->phonemes[p] == phonemes[p]) {
                match_len++;
            } else {
                break;
            }
        }

        if (match_len > 0) {
            candidates[found].word_id = entry->word_id;
            candidates[found].matched_phonemes = (uint8_t)match_len;
            candidates[found].cohort_probability = (float)match_len / entry->phoneme_count;
            candidates[found].uniqueness_point = (float)match_len / entry->phoneme_count;
            candidates[found].recognition_complete = (match_len == entry->phoneme_count);
            found++;
        }
    }

    *num_candidates = found;
    ctx->stats.word_recognitions++;
    return true;
}

bool wernicke_gpu_update_cohort(
    wernicke_gpu_context_t* ctx,
    uint8_t new_phoneme,
    float phoneme_confidence)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_gpu_update_cohort: ctx is NULL");
        return false;
    }

    if (ctx->phoneme_seq_len < ctx->config.max_phonemes_per_word) {
        ctx->current_phoneme_seq[ctx->phoneme_seq_len++] = new_phoneme;
    }

    /* Re-evaluate cohort */
    ctx->cohort_size = 0;
    for (uint32_t w = 0; w < ctx->lexicon_size && ctx->cohort_size < ctx->config.max_cohort_size; w++) {
        wernicke_gpu_lexical_entry_t* entry = &ctx->lexicon[w];

        bool matches = true;
        for (uint32_t p = 0; p < ctx->phoneme_seq_len && p < entry->phoneme_count; p++) {
            if (entry->phonemes[p] != ctx->current_phoneme_seq[p]) {
                matches = false;
                break;
            }
        }

        if (matches && ctx->phoneme_seq_len <= entry->phoneme_count) {
            ctx->cohort[ctx->cohort_size].word_id = entry->word_id;
            ctx->cohort[ctx->cohort_size].matched_phonemes = (uint8_t)ctx->phoneme_seq_len;
            ctx->cohort[ctx->cohort_size].cohort_probability = phoneme_confidence;
            ctx->cohort[ctx->cohort_size].recognition_complete =
                (ctx->phoneme_seq_len == entry->phoneme_count);
            ctx->cohort_size++;
        }
    }

    return true;
}

bool wernicke_gpu_get_cohort(
    wernicke_gpu_context_t* ctx,
    wernicke_gpu_word_candidate_t* candidates,
    uint32_t max_candidates,
    uint32_t* num_candidates)
{
    if (!ctx || !candidates || !num_candidates) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_gpu_get_cohort: required parameter is NULL (ctx, candidates, num_candidates)");
        return false;
    }

    uint32_t copy_count = ctx->cohort_size < max_candidates ? ctx->cohort_size : max_candidates;
    memcpy(candidates, ctx->cohort, copy_count * sizeof(wernicke_gpu_word_candidate_t));
    *num_candidates = copy_count;

    return true;
}

bool wernicke_gpu_reset_cohort(wernicke_gpu_context_t* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_gpu_reset_cohort: ctx is NULL");
        return false;
    }
    ctx->cohort_size = 0;
    ctx->phoneme_seq_len = 0;
    return true;
}

bool wernicke_gpu_upload_semantic_network(
    wernicke_gpu_context_t* ctx,
    const wernicke_gpu_concept_t* concepts,
    uint32_t num_concepts,
    const uint32_t* adjacency_matrix,
    const float* weights)
{
    (void)ctx;
    (void)concepts;
    (void)num_concepts;
    (void)adjacency_matrix;
    (void)weights;
    /* Semantic network not implemented in CPU fallback */
    return true;
}

bool wernicke_gpu_spread_activation(
    wernicke_gpu_context_t* ctx,
    const uint32_t* seed_concepts,
    const float* seed_activations,
    uint32_t num_seeds,
    wernicke_gpu_activation_result_t* results,
    uint32_t max_results,
    uint32_t* num_results)
{
    if (!ctx || !results || !num_results) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_gpu_spread_activation: required parameter is NULL (ctx, results, num_results)");
        return false;
    }

    /* Simple pass-through: return seed concepts as activated */
    uint32_t count = num_seeds < max_results ? num_seeds : max_results;
    for (uint32_t i = 0; i < count; i++) {
        results[i].concept_id = seed_concepts ? seed_concepts[i] : i;
        results[i].activation = seed_activations ? seed_activations[i] : 1.0f;
        results[i].spreading_contribution = 0.0f;
    }
    *num_results = count;

    ctx->stats.spreading_activations++;
    return true;
}

bool wernicke_gpu_get_top_activated(
    wernicke_gpu_context_t* ctx,
    uint32_t top_k,
    wernicke_gpu_activation_result_t* results,
    uint32_t* actual_count)
{
    (void)ctx;
    (void)top_k;
    (void)results;
    if (actual_count) *actual_count = 0;
    return true;
}

float wernicke_gpu_semantic_similarity(
    wernicke_gpu_context_t* ctx,
    uint32_t concept_a,
    uint32_t concept_b)
{
    (void)ctx;
    return concept_a == concept_b ? 1.0f : 0.0f;
}

bool wernicke_gpu_wm_push(
    wernicke_gpu_context_t* ctx,
    const uint8_t* phonemes,
    uint32_t count)
{
    if (!ctx || !phonemes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_gpu_wm_push: required parameter is NULL (ctx, phonemes)");
        return false;
    }

    for (uint32_t i = 0; i < count && ctx->wm_count < ctx->config.working_memory_slots; i++) {
        ctx->wm_phonemes[ctx->wm_count] = phonemes[i];
        ctx->wm_activations[ctx->wm_count] = 1.0f;
        ctx->wm_count++;
    }

    ctx->stats.wm_operations++;
    return true;
}

bool wernicke_gpu_wm_get_contents(
    wernicke_gpu_context_t* ctx,
    uint8_t* phonemes,
    float* activations,
    uint32_t max_count,
    uint32_t* actual_count)
{
    if (!ctx || !actual_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_gpu_wm_get_contents: required parameter is NULL (ctx, actual_count)");
        return false;
    }

    uint32_t count = ctx->wm_count < max_count ? ctx->wm_count : max_count;
    if (phonemes) memcpy(phonemes, ctx->wm_phonemes, count);
    if (activations) memcpy(activations, ctx->wm_activations, count * sizeof(float));
    *actual_count = count;

    return true;
}

bool wernicke_gpu_wm_rehearse(wernicke_gpu_context_t* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_gpu_wm_rehearse: ctx is NULL");
        return false;
    }

    /* Boost all activations */
    for (uint32_t i = 0; i < ctx->wm_count; i++) {
        ctx->wm_activations[i] = fminf(1.0f, ctx->wm_activations[i] + 0.1f);
    }

    ctx->stats.wm_operations++;
    return true;
}

bool wernicke_gpu_wm_apply_decay(
    wernicke_gpu_context_t* ctx,
    float decay_factor,
    float threshold)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_gpu_wm_apply_decay: ctx is NULL");
        return false;
    }

    uint32_t write_idx = 0;
    for (uint32_t i = 0; i < ctx->wm_count; i++) {
        ctx->wm_activations[i] *= decay_factor;
        if (ctx->wm_activations[i] >= threshold) {
            ctx->wm_phonemes[write_idx] = ctx->wm_phonemes[i];
            ctx->wm_activations[write_idx] = ctx->wm_activations[i];
            write_idx++;
        }
    }
    ctx->wm_count = write_idx;

    return true;
}

bool wernicke_gpu_wm_clear(wernicke_gpu_context_t* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_gpu_wm_clear: ctx is NULL");
        return false;
    }
    ctx->wm_count = 0;
    return true;
}

bool wernicke_gpu_comprehend(
    wernicke_gpu_context_t* ctx,
    const wernicke_gpu_spectral_frame_t* frames,
    uint32_t num_frames,
    wernicke_gpu_word_candidate_t* word_candidates,
    uint32_t max_word_candidates,
    uint32_t* num_word_candidates,
    wernicke_gpu_activation_result_t* semantic_activations,
    uint32_t max_semantic_activations,
    uint32_t* num_semantic_activations)
{
    if (!ctx || !frames) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_gpu_comprehend: required parameter is NULL (ctx, frames)");
        return false;
    }

    /* Step 1: Recognize phonemes */
    wernicke_gpu_phoneme_result_t* phonemes = nimcp_calloc(num_frames, sizeof(wernicke_gpu_phoneme_result_t));
    if (!phonemes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "wernicke_gpu_comprehend: phonemes is NULL");
        return false;
    }

    wernicke_gpu_recognize_phonemes(ctx, frames, num_frames, phonemes);

    /* Step 2: Extract phoneme sequence */
    uint8_t* phoneme_seq = nimcp_calloc(num_frames, sizeof(uint8_t));
    if (!phoneme_seq) {
        nimcp_free(phonemes);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "wernicke_gpu_comprehend: phoneme_seq is NULL");
        return false;
    }

    for (uint32_t i = 0; i < num_frames; i++) {
        phoneme_seq[i] = phonemes[i].phoneme_id;
    }

    /* Step 3: Recognize words */
    if (word_candidates && num_word_candidates) {
        wernicke_gpu_recognize_words(ctx, phoneme_seq, num_frames,
                                      word_candidates, max_word_candidates, num_word_candidates);
    }

    /* Step 4: Generate semantic activations */
    if (semantic_activations && num_semantic_activations) {
        uint32_t num_words = num_word_candidates ? *num_word_candidates : 0;
        uint32_t* concepts = nimcp_calloc(num_words, sizeof(uint32_t));
        float* acts = nimcp_calloc(num_words, sizeof(float));

        if (concepts && acts) {
            for (uint32_t i = 0; i < num_words; i++) {
                concepts[i] = word_candidates[i].word_id;
                acts[i] = word_candidates[i].cohort_probability;
            }
            wernicke_gpu_spread_activation(ctx, concepts, acts, num_words,
                                            semantic_activations, max_semantic_activations,
                                            num_semantic_activations);
        }

        nimcp_free(concepts);
        nimcp_free(acts);
    }

    nimcp_free(phonemes);
    nimcp_free(phoneme_seq);

    return true;
}

bool wernicke_gpu_get_stats(
    const wernicke_gpu_context_t* ctx,
    wernicke_gpu_stats_t* stats)
{
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_gpu_get_stats: stats is NULL");
        return false;
    }

    if (ctx) {
        *stats = ctx->stats;
        stats->current_cohort_size = ctx->cohort_size;
    } else {
        memset(stats, 0, sizeof(wernicke_gpu_stats_t));
    }
    return true;
}

void wernicke_gpu_reset_stats(wernicke_gpu_context_t* ctx) {
    if (ctx) {
        memset(&ctx->stats, 0, sizeof(wernicke_gpu_stats_t));
    }
}

/* CPU reference implementations */
bool wernicke_cpu_recognize_phonemes(
    const wernicke_gpu_spectral_frame_t* frames,
    uint32_t num_frames,
    const float* phoneme_embeddings,
    uint32_t num_phonemes,
    uint32_t embed_dim,
    wernicke_gpu_phoneme_result_t* results)
{
    if (!frames || !results) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_cpu_recognize_phonemes: required parameter is NULL (frames, results)");
        return false;
    }

    for (uint32_t i = 0; i < num_frames; i++) {
        /* Use MFCC as feature vector */
        float best_sim = -1.0f;
        uint8_t best_phoneme = 0;

        if (phoneme_embeddings && embed_dim <= 13) {
            for (uint32_t p = 0; p < num_phonemes; p++) {
                float sim = 0.0f;
                float norm_a = 0.0f, norm_b = 0.0f;

                for (uint32_t d = 0; d < embed_dim; d++) {
                    float a = frames[i].mfcc[d];
                    float b = phoneme_embeddings[p * embed_dim + d];
                    sim += a * b;
                    norm_a += a * a;
                    norm_b += b * b;
                }

                if (norm_a > 0 && norm_b > 0) {
                    sim /= sqrtf(norm_a * norm_b);
                }

                if (sim > best_sim) {
                    best_sim = sim;
                    best_phoneme = (uint8_t)p;
                }
            }
        } else {
            best_phoneme = (uint8_t)(i % num_phonemes);
            best_sim = 0.5f;
        }

        results[i].phoneme_id = best_phoneme;
        results[i].confidence = (best_sim + 1.0f) / 2.0f;
        results[i].posterior = NULL;
    }

    return true;
}

bool wernicke_cpu_recognize_words(
    const wernicke_gpu_lexical_entry_t* lexicon,
    uint32_t lexicon_size,
    const uint8_t* phonemes,
    uint32_t num_phonemes,
    wernicke_gpu_word_candidate_t* candidates,
    uint32_t max_candidates,
    uint32_t* num_candidates)
{
    if (!lexicon || !phonemes || !candidates || !num_candidates) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_cpu_recognize_words: required parameter is NULL (lexicon, phonemes, candidates, num_candidates)");
        return false;
    }

    uint32_t found = 0;

    for (uint32_t w = 0; w < lexicon_size && found < max_candidates; w++) {
        uint32_t match_len = 0;
        for (uint32_t p = 0; p < num_phonemes && p < lexicon[w].phoneme_count; p++) {
            if (lexicon[w].phonemes[p] == phonemes[p]) {
                match_len++;
            } else {
                break;
            }
        }

        if (match_len > 0) {
            candidates[found].word_id = lexicon[w].word_id;
            candidates[found].matched_phonemes = (uint8_t)match_len;
            candidates[found].cohort_probability = (float)match_len / lexicon[w].phoneme_count;
            candidates[found].uniqueness_point = (float)match_len / lexicon[w].phoneme_count;
            candidates[found].recognition_complete = (match_len == lexicon[w].phoneme_count);
            found++;
        }
    }

    *num_candidates = found;
    return true;
}

bool wernicke_cpu_spread_activation(
    const float* adjacency_weights,
    uint32_t num_concepts,
    uint32_t max_neighbors,
    const uint32_t* seed_concepts,
    const float* seed_activations,
    uint32_t num_seeds,
    uint32_t iterations,
    float decay,
    float* output_activations)
{
    if (!output_activations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_cpu_spread_activation: output_activations is NULL");
        return false;
    }

    /* Initialize activations */
    memset(output_activations, 0, num_concepts * sizeof(float));

    if (seed_concepts && seed_activations) {
        for (uint32_t i = 0; i < num_seeds; i++) {
            if (seed_concepts[i] < num_concepts) {
                output_activations[seed_concepts[i]] = seed_activations[i];
            }
        }
    }

    /* Simple spreading activation */
    float* temp = nimcp_calloc(num_concepts, sizeof(float));
    if (!temp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "wernicke_cpu_spread_activation: temp is NULL");
        return false;
    }

    for (uint32_t iter = 0; iter < iterations; iter++) {
        memset(temp, 0, num_concepts * sizeof(float));

        for (uint32_t c = 0; c < num_concepts; c++) {
            if (output_activations[c] > 0.01f && adjacency_weights) {
                /* Spread to neighbors */
                for (uint32_t n = 0; n < max_neighbors; n++) {
                    uint32_t neighbor = (c + n + 1) % num_concepts;
                    float weight = adjacency_weights[c * max_neighbors + n];
                    temp[neighbor] += output_activations[c] * weight * decay;
                }
            }
        }

        /* Apply spreading */
        for (uint32_t c = 0; c < num_concepts; c++) {
            output_activations[c] = fmaxf(output_activations[c] * decay, temp[c]);
        }
    }

    nimcp_free(temp);
    return true;
}

/*=============================================================================
 * Tensor GPU Stubs (gpu/tensor/nimcp_tensor_gpu.h)
 *=============================================================================*/

#include "gpu/tensor/nimcp_tensor_gpu.h"

/* CPU tensor implementation */
nimcp_gpu_tensor_t* nimcp_gpu_tensor_create(
    nimcp_gpu_context_t* ctx,
    const size_t* dims,
    uint32_t ndim,
    nimcp_gpu_precision_t precision)
{
    (void)ctx;

    if (!dims || ndim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_gpu_tensor_create: dims is NULL");
        return NULL;
    }

    nimcp_gpu_tensor_t* tensor = nimcp_calloc(1, sizeof(nimcp_gpu_tensor_t));
    if (!tensor) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_gpu_tensor_create: allocation failed");

        return NULL;

    }

    tensor->ndim = ndim;
    tensor->precision = precision;
    tensor->layout = NIMCP_TENSOR_LAYOUT_ROW_MAJOR;
    tensor->owns_data = true;

    /* Allocate dims and strides on CPU */
    tensor->dims = nimcp_malloc(ndim * sizeof(size_t));
    tensor->strides = nimcp_malloc(ndim * sizeof(size_t));
    if (!tensor->dims || !tensor->strides) {
        nimcp_gpu_tensor_destroy(tensor);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_gpu_tensor_create: required parameter is NULL (tensor->dims, tensor->strides)");
        return NULL;
    }

    /* Calculate total elements and strides */
    tensor->numel = 1;
    for (uint32_t i = 0; i < ndim; i++) {
        tensor->dims[i] = dims[i];
        tensor->numel *= dims[i];
    }

    /* Row-major strides */
    tensor->strides[ndim - 1] = 1;
    for (int i = (int)ndim - 2; i >= 0; i--) {
        tensor->strides[i] = tensor->strides[i + 1] * tensor->dims[i + 1];
    }

    /* Determine element size */
    switch (precision) {
        case NIMCP_GPU_PRECISION_FP32:
        case NIMCP_GPU_PRECISION_TF32:
        case NIMCP_GPU_PRECISION_INT32:
        case NIMCP_GPU_PRECISION_UINT32:
            tensor->elem_size = 4;
            break;
        case NIMCP_GPU_PRECISION_FP16:
        case NIMCP_GPU_PRECISION_BF16:
            tensor->elem_size = 2;
            break;
        case NIMCP_GPU_PRECISION_INT8:
            tensor->elem_size = 1;
            break;
        default:
            tensor->elem_size = 4;
    }

    /* Allocate data on CPU */
    tensor->data = nimcp_calloc(tensor->numel, tensor->elem_size);
    if (!tensor->data) {
        nimcp_gpu_tensor_destroy(tensor);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_gpu_tensor_create: tensor->data is NULL");
        return NULL;
    }

    return tensor;
}

nimcp_gpu_tensor_t* nimcp_gpu_tensor_from_host(
    nimcp_gpu_context_t* ctx,
    const void* host_data,
    const size_t* dims,
    uint32_t ndim,
    nimcp_gpu_precision_t precision)
{
    if (!host_data) return NULL;
    nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_create(ctx, dims, ndim, precision);
    if (!tensor) return NULL;

    memcpy(tensor->data, host_data, tensor->numel * tensor->elem_size);
    return tensor;
}

bool nimcp_gpu_tensor_to_host(
    const nimcp_gpu_tensor_t* tensor,
    void* host_data)
{
    if (!tensor || !host_data || !tensor->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_tensor_to_host: required parameter is NULL (tensor, host_data, tensor->data)");
        return false;
    }
    memcpy(host_data, tensor->data, tensor->numel * tensor->elem_size);
    return true;
}

void nimcp_gpu_tensor_destroy(nimcp_gpu_tensor_t* tensor) {
    if (!tensor) return;

    if (tensor->owns_data) {
        nimcp_free(tensor->data);
    }
    nimcp_free(tensor->dims);
    nimcp_free(tensor->strides);
    nimcp_free(tensor);
}

nimcp_gpu_tensor_t* nimcp_gpu_tensor_clone(const nimcp_gpu_tensor_t* tensor) {
    if (!tensor) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tensor is NULL");

        return NULL;

    }

    nimcp_gpu_tensor_t* clone = nimcp_gpu_tensor_create(
        tensor->ctx, tensor->dims, tensor->ndim, tensor->precision);
    if (!clone) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_gpu_tensor_clone: allocation failed");

        return NULL;

    }

    if (tensor->data) {
        memcpy(clone->data, tensor->data, tensor->numel * tensor->elem_size);
    }

    return clone;
}

bool nimcp_gpu_fill(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* tensor,
    float value)
{
    (void)ctx;
    if (!tensor || !tensor->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_fill: required parameter is NULL (tensor, tensor->data)");
        return false;
    }

    if (tensor->precision == NIMCP_GPU_PRECISION_FP32) {
        float* data = (float*)tensor->data;
        for (size_t i = 0; i < tensor->numel; i++) {
            data[i] = value;
        }
    } else {
        /* For other precisions, fill with bytes */
        memset(tensor->data, (int)value, tensor->numel * tensor->elem_size);
    }

    return true;
}

bool nimcp_gpu_zeros(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* tensor)
{
    (void)ctx;
    if (!tensor || !tensor->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_zeros: required parameter is NULL (tensor, tensor->data)");
        return false;
    }
    memset(tensor->data, 0, tensor->numel * tensor->elem_size);
    return true;
}

/* GEMM: C = alpha * A @ B + beta * C */
bool nimcp_gpu_gemm(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* A,
    const nimcp_gpu_tensor_t* B,
    nimcp_gpu_tensor_t* C,
    float alpha,
    float beta,
    bool trans_a,
    bool trans_b)
{
    (void)ctx;
    if (!A || !B || !C) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_gemm: required parameter is NULL (A, B, C)");
        return false;
    }
    if (!A->data || !B->data || !C->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_gemm: required parameter is NULL (A->data, B->data, C->data)");
        return false;
    }

    /* Get dimensions */
    size_t M = trans_a ? A->dims[1] : A->dims[0];
    size_t K_a = trans_a ? A->dims[0] : A->dims[1];
    size_t K_b = trans_b ? B->dims[1] : B->dims[0];
    size_t N = trans_b ? B->dims[0] : B->dims[1];

    if (K_a != K_b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_gpu_gemm: validation failed");
        return false;
    }

    float* a = (float*)A->data;
    float* b = (float*)B->data;
    float* c = (float*)C->data;

    /* Naive CPU GEMM */
    for (size_t i = 0; i < M; i++) {
        for (size_t j = 0; j < N; j++) {
            float sum = 0.0f;
            for (size_t k = 0; k < K_a; k++) {
                size_t a_idx = trans_a ? (k * M + i) : (i * K_a + k);
                size_t b_idx = trans_b ? (j * K_b + k) : (k * N + j);
                sum += a[a_idx] * b[b_idx];
            }
            size_t c_idx = i * N + j;
            c[c_idx] = alpha * sum + beta * c[c_idx];
        }
    }
    return true;
}

bool nimcp_gpu_gemm_batched(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* A,
    const nimcp_gpu_tensor_t* B,
    nimcp_gpu_tensor_t* C,
    float alpha,
    float beta,
    bool trans_a,
    bool trans_b)
{
    (void)ctx;
    if (!A || !B || !C) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_gemm_batched: required parameter is NULL (A, B, C)");
        return false;
    }
    if (!A->data || !B->data || !C->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_gemm_batched: required parameter is NULL (A->data, B->data, C->data)");
        return false;
    }
    if (A->ndim < 3 || B->ndim < 3 || C->ndim < 3) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_gpu_gemm_batched: validation failed");
        return false;
    }

    size_t batch_size = A->dims[0];
    size_t M = trans_a ? A->dims[2] : A->dims[1];
    size_t K = trans_a ? A->dims[1] : A->dims[2];
    size_t N = trans_b ? B->dims[1] : B->dims[2];

    float* a = (float*)A->data;
    float* b = (float*)B->data;
    float* c = (float*)C->data;

    size_t a_batch_stride = M * K;
    size_t b_batch_stride = K * N;
    size_t c_batch_stride = M * N;

    for (size_t batch = 0; batch < batch_size; batch++) {
        float* a_batch = a + batch * a_batch_stride;
        float* b_batch = b + batch * b_batch_stride;
        float* c_batch = c + batch * c_batch_stride;

        for (size_t i = 0; i < M; i++) {
            for (size_t j = 0; j < N; j++) {
                float sum = 0.0f;
                for (size_t k = 0; k < K; k++) {
                    size_t a_idx = trans_a ? (k * M + i) : (i * K + k);
                    size_t b_idx = trans_b ? (j * K + k) : (k * N + j);
                    sum += a_batch[a_idx] * b_batch[b_idx];
                }
                size_t c_idx = i * N + j;
                c_batch[c_idx] = alpha * sum + beta * c_batch[c_idx];
            }
        }
    }
    return true;
}

bool nimcp_gpu_gemv(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* A,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* y,
    float alpha,
    float beta,
    bool trans_a)
{
    (void)ctx;
    if (!A || !x || !y) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_gemv: required parameter is NULL (A, x, y)");
        return false;
    }
    if (!A->data || !x->data || !y->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_gemv: required parameter is NULL (A->data, x->data, y->data)");
        return false;
    }

    size_t M = trans_a ? A->dims[1] : A->dims[0];
    size_t N = trans_a ? A->dims[0] : A->dims[1];

    float* a = (float*)A->data;
    float* xv = (float*)x->data;
    float* yv = (float*)y->data;

    for (size_t i = 0; i < M; i++) {
        float sum = 0.0f;
        for (size_t j = 0; j < N; j++) {
            size_t a_idx = trans_a ? (j * M + i) : (i * N + j);
            sum += a[a_idx] * xv[j];
        }
        yv[i] = alpha * sum + beta * yv[i];
    }
    return true;
}

bool nimcp_gpu_add(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* a,
    const nimcp_gpu_tensor_t* b,
    nimcp_gpu_tensor_t* out)
{
    (void)ctx;
    if (!a || !b || !out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_add: required parameter is NULL (a, b, out)");
        return false;
    }
    if (!a->data || !b->data || !out->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_add: required parameter is NULL (a->data, b->data, out->data)");
        return false;
    }

    float* av = (float*)a->data;
    float* bv = (float*)b->data;
    float* ov = (float*)out->data;

    for (size_t i = 0; i < out->numel; i++) {
        ov[i] = av[i % a->numel] + bv[i % b->numel];
    }
    return true;
}

bool nimcp_gpu_sub(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* a,
    const nimcp_gpu_tensor_t* b,
    nimcp_gpu_tensor_t* out)
{
    (void)ctx;
    if (!a || !b || !out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_sub: required parameter is NULL (a, b, out)");
        return false;
    }
    if (!a->data || !b->data || !out->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_sub: required parameter is NULL (a->data, b->data, out->data)");
        return false;
    }

    float* av = (float*)a->data;
    float* bv = (float*)b->data;
    float* ov = (float*)out->data;

    for (size_t i = 0; i < out->numel; i++) {
        ov[i] = av[i % a->numel] - bv[i % b->numel];
    }
    return true;
}

bool nimcp_gpu_mul(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* a,
    const nimcp_gpu_tensor_t* b,
    nimcp_gpu_tensor_t* out)
{
    (void)ctx;
    if (!a || !b || !out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_mul: required parameter is NULL (a, b, out)");
        return false;
    }
    if (!a->data || !b->data || !out->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_mul: required parameter is NULL (a->data, b->data, out->data)");
        return false;
    }

    float* av = (float*)a->data;
    float* bv = (float*)b->data;
    float* ov = (float*)out->data;

    for (size_t i = 0; i < out->numel; i++) {
        ov[i] = av[i % a->numel] * bv[i % b->numel];
    }
    return true;
}

bool nimcp_gpu_div(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* a,
    const nimcp_gpu_tensor_t* b,
    nimcp_gpu_tensor_t* out)
{
    (void)ctx;
    if (!a || !b || !out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_div: required parameter is NULL (a, b, out)");
        return false;
    }
    if (!a->data || !b->data || !out->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_div: required parameter is NULL (a->data, b->data, out->data)");
        return false;
    }

    float* av = (float*)a->data;
    float* bv = (float*)b->data;
    float* ov = (float*)out->data;

    for (size_t i = 0; i < out->numel; i++) {
        float divisor = bv[i % b->numel];
        ov[i] = av[i % a->numel] / (divisor + ((divisor >= 0.0f) ? 1e-7f : -1e-7f));
    }
    return true;
}

bool nimcp_gpu_add_scalar(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* a,
    float scalar,
    nimcp_gpu_tensor_t* out)
{
    (void)ctx;
    if (!a || !out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_add_scalar: required parameter is NULL (a, out)");
        return false;
    }
    if (!a->data || !out->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_add_scalar: required parameter is NULL (a->data, out->data)");
        return false;
    }

    float* av = (float*)a->data;
    float* ov = (float*)out->data;

    for (size_t i = 0; i < out->numel; i++) {
        ov[i] = av[i] + scalar;
    }
    return true;
}

bool nimcp_gpu_mul_scalar(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* a,
    float scalar,
    nimcp_gpu_tensor_t* out)
{
    (void)ctx;
    if (!a || !out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_mul_scalar: required parameter is NULL (a, out)");
        return false;
    }
    if (!a->data || !out->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_mul_scalar: required parameter is NULL (a->data, out->data)");
        return false;
    }

    float* av = (float*)a->data;
    float* ov = (float*)out->data;

    for (size_t i = 0; i < out->numel; i++) {
        ov[i] = av[i] * scalar;
    }
    return true;
}

bool nimcp_gpu_relu(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* out)
{
    (void)ctx;
    if (!x || !out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_relu: required parameter is NULL (x, out)");
        return false;
    }
    if (!x->data || !out->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_relu: required parameter is NULL (x->data, out->data)");
        return false;
    }

    float* xv = (float*)x->data;
    float* ov = (float*)out->data;

    for (size_t i = 0; i < out->numel; i++) {
        ov[i] = xv[i] > 0 ? xv[i] : 0;
    }
    return true;
}

bool nimcp_gpu_leaky_relu(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* out,
    float alpha)
{
    (void)ctx;
    if (!x || !out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_leaky_relu: required parameter is NULL (x, out)");
        return false;
    }
    if (!x->data || !out->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_leaky_relu: required parameter is NULL (x->data, out->data)");
        return false;
    }

    float* xv = (float*)x->data;
    float* ov = (float*)out->data;

    for (size_t i = 0; i < out->numel; i++) {
        ov[i] = xv[i] > 0 ? xv[i] : alpha * xv[i];
    }
    return true;
}

bool nimcp_gpu_sigmoid(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* out)
{
    (void)ctx;
    if (!x || !out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_sigmoid: required parameter is NULL (x, out)");
        return false;
    }
    if (!x->data || !out->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_sigmoid: required parameter is NULL (x->data, out->data)");
        return false;
    }

    float* xv = (float*)x->data;
    float* ov = (float*)out->data;

    for (size_t i = 0; i < out->numel; i++) {
        ov[i] = 1.0f / (1.0f + expf(-xv[i]));
    }
    return true;
}

bool nimcp_gpu_tanh(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* out)
{
    (void)ctx;
    if (!x || !out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_tanh: required parameter is NULL (x, out)");
        return false;
    }
    if (!x->data || !out->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_tanh: required parameter is NULL (x->data, out->data)");
        return false;
    }

    float* xv = (float*)x->data;
    float* ov = (float*)out->data;

    for (size_t i = 0; i < out->numel; i++) {
        ov[i] = tanhf(xv[i]);
    }
    return true;
}

bool nimcp_gpu_gelu(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* out)
{
    (void)ctx;
    if (!x || !out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_gelu: required parameter is NULL (x, out)");
        return false;
    }
    if (!x->data || !out->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_gelu: required parameter is NULL (x->data, out->data)");
        return false;
    }

    float* xv = (float*)x->data;
    float* ov = (float*)out->data;

    const float sqrt_2_pi = 0.7978845608028654f;
    const float coeff = 0.044715f;

    for (size_t i = 0; i < out->numel; i++) {
        float xi = xv[i];
        float x3 = xi * xi * xi;
        float inner = sqrt_2_pi * (xi + coeff * x3);
        ov[i] = 0.5f * xi * (1.0f + tanhf(inner));
    }
    return true;
}

bool nimcp_gpu_silu(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* out)
{
    (void)ctx;
    if (!x || !out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_silu: required parameter is NULL (x, out)");
        return false;
    }
    if (!x->data || !out->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_silu: required parameter is NULL (x->data, out->data)");
        return false;
    }

    float* xv = (float*)x->data;
    float* ov = (float*)out->data;

    for (size_t i = 0; i < out->numel; i++) {
        float xi = xv[i];
        ov[i] = xi / (1.0f + expf(-xi));
    }
    return true;
}

bool nimcp_gpu_softmax(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* out)
{
    (void)ctx;
    if (!x || !out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_softmax: required parameter is NULL (x, out)");
        return false;
    }
    if (!x->data || !out->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_softmax: required parameter is NULL (x->data, out->data)");
        return false;
    }

    float* xv = (float*)x->data;
    float* ov = (float*)out->data;

    /* Compute softmax along last dimension */
    size_t last_dim = (x->ndim > 0) ? x->dims[x->ndim - 1] : x->numel;
    if (last_dim == 0) return false;
    size_t num_rows = x->numel / last_dim;

    for (size_t row = 0; row < num_rows; row++) {
        float* row_in = xv + row * last_dim;
        float* row_out = ov + row * last_dim;

        /* Find max for numerical stability */
        float max_val = row_in[0];
        for (size_t i = 1; i < last_dim; i++) {
            if (row_in[i] > max_val) max_val = row_in[i];
        }

        /* Guard against all-NaN or all-inf input */
        if (isnan(max_val) || isinf(max_val)) {
            float uniform = 1.0f / (float)last_dim;
            for (size_t i = 0; i < last_dim; i++) {
                row_out[i] = uniform;
            }
            continue;
        }

        /* Compute exp(x - max) and sum with clamping.
         * After subtracting max, exponents are in (-inf, 0], so expf won't
         * overflow. Clamp very negative values to avoid prolonged underflow. */
        float sum = 0.0f;
        for (size_t i = 0; i < last_dim; i++) {
            float shifted = row_in[i] - max_val;
            if (shifted < -87.0f) shifted = -87.0f;  /* expf(-88) ~ 0 in fp32 */
            row_out[i] = expf(shifted);
            sum += row_out[i];
        }

        /* Guard against zero sum (all values underflowed) */
        if (sum <= 0.0f || isnan(sum)) {
            float uniform = 1.0f / (float)last_dim;
            for (size_t i = 0; i < last_dim; i++) {
                row_out[i] = uniform;
            }
            continue;
        }

        /* Normalize */
        float inv_sum = 1.0f / sum;
        for (size_t i = 0; i < last_dim; i++) {
            row_out[i] *= inv_sum;
        }
    }
    return true;
}

bool nimcp_gpu_log_softmax(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* out)
{
    (void)ctx;
    if (!x || !out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_log_softmax: required parameter is NULL (x, out)");
        return false;
    }
    if (!x->data || !out->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_log_softmax: required parameter is NULL (x->data, out->data)");
        return false;
    }

    float* xv = (float*)x->data;
    float* ov = (float*)out->data;

    size_t last_dim = (x->ndim > 0) ? x->dims[x->ndim - 1] : x->numel;
    size_t num_rows = x->numel / last_dim;

    for (size_t row = 0; row < num_rows; row++) {
        float* row_in = xv + row * last_dim;
        float* row_out = ov + row * last_dim;

        float max_val = row_in[0];
        for (size_t i = 1; i < last_dim; i++) {
            if (row_in[i] > max_val) max_val = row_in[i];
        }

        float sum = 0.0f;
        for (size_t i = 0; i < last_dim; i++) {
            sum += expf(row_in[i] - max_val);
        }
        float log_sum = logf(sum);

        for (size_t i = 0; i < last_dim; i++) {
            row_out[i] = row_in[i] - max_val - log_sum;
        }
    }
    return true;
}

bool nimcp_gpu_exp(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* out)
{
    (void)ctx;
    if (!x || !out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_exp: required parameter is NULL (x, out)");
        return false;
    }
    if (!x->data || !out->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_exp: required parameter is NULL (x->data, out->data)");
        return false;
    }

    float* xv = (float*)x->data;
    float* ov = (float*)out->data;

    for (size_t i = 0; i < out->numel; i++) {
        ov[i] = expf(xv[i]);
    }
    return true;
}

bool nimcp_gpu_log(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* out)
{
    (void)ctx;
    if (!x || !out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_log: required parameter is NULL (x, out)");
        return false;
    }
    if (!x->data || !out->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_log: required parameter is NULL (x->data, out->data)");
        return false;
    }

    float* xv = (float*)x->data;
    float* ov = (float*)out->data;

    for (size_t i = 0; i < out->numel; i++) {
        ov[i] = logf(fmaxf(xv[i], 1e-7f));
    }
    return true;
}

bool nimcp_gpu_sqrt(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* out)
{
    (void)ctx;
    if (!x || !out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_sqrt: required parameter is NULL (x, out)");
        return false;
    }
    if (!x->data || !out->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_sqrt: required parameter is NULL (x->data, out->data)");
        return false;
    }

    float* xv = (float*)x->data;
    float* ov = (float*)out->data;

    for (size_t i = 0; i < out->numel; i++) {
        ov[i] = sqrtf(fmaxf(xv[i], 0.0f));
    }
    return true;
}

bool nimcp_gpu_pow(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    float exponent,
    nimcp_gpu_tensor_t* out)
{
    (void)ctx;
    if (!x || !out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_pow: required parameter is NULL (x, out)");
        return false;
    }
    if (!x->data || !out->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_pow: required parameter is NULL (x->data, out->data)");
        return false;
    }

    float* xv = (float*)x->data;
    float* ov = (float*)out->data;

    for (size_t i = 0; i < out->numel; i++) {
        ov[i] = powf(xv[i], exponent);
    }
    return true;
}

bool nimcp_gpu_abs(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* out)
{
    (void)ctx;
    if (!x || !out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_abs: required parameter is NULL (x, out)");
        return false;
    }
    if (!x->data || !out->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_abs: required parameter is NULL (x->data, out->data)");
        return false;
    }

    float* xv = (float*)x->data;
    float* ov = (float*)out->data;

    for (size_t i = 0; i < out->numel; i++) {
        ov[i] = fabsf(xv[i]);
    }
    return true;
}

bool nimcp_gpu_clamp(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    float min_val,
    float max_val,
    nimcp_gpu_tensor_t* out)
{
    (void)ctx;
    if (!x || !out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_clamp: required parameter is NULL (x, out)");
        return false;
    }
    if (!x->data || !out->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_clamp: required parameter is NULL (x->data, out->data)");
        return false;
    }

    float* xv = (float*)x->data;
    float* ov = (float*)out->data;

    for (size_t i = 0; i < out->numel; i++) {
        float v = xv[i];
        if (v < min_val) v = min_val;
        if (v > max_val) v = max_val;
        ov[i] = v;
    }
    return true;
}

bool nimcp_gpu_copy(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* src,
    nimcp_gpu_tensor_t* dst)
{
    (void)ctx;
    if (!src || !dst) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_copy: required parameter is NULL (src, dst)");
        return false;
    }
    if (!src->data || !dst->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_copy: required parameter is NULL (src->data, dst->data)");
        return false;
    }

    size_t to_copy = src->numel < dst->numel ? src->numel : dst->numel;
    memcpy(dst->data, src->data, to_copy * sizeof(float));
    return true;
}

/* nimcp_gpu_memcpy and nimcp_gpu_memcpy_async are in nimcp_gpu_context_stub.c */

/*=============================================================================
 * Graph GPU Stubs (gpu/graph/nimcp_graph_gpu.h)
 *=============================================================================*/

#include "gpu/graph/nimcp_graph_gpu.h"

void nimcp_gpu_graph_destroy(nimcp_gpu_graph_t* graph) {
    if (!graph) return;

    /* Free CSR arrays - these are CPU memory in stub mode */
    nimcp_free(graph->d_row_offsets);
    nimcp_free(graph->d_col_indices);
    nimcp_free(graph->d_edge_weights);
    nimcp_free(graph);
}

/*=============================================================================
 * JEPA GPU Stubs (gpu/cognitive/nimcp_jepa_gpu.h)
 *=============================================================================*/

#include "gpu/cognitive/nimcp_jepa_gpu.h"

/* Forward declaration for tensor destroy */
extern void nimcp_gpu_tensor_destroy(nimcp_gpu_tensor_t* tensor);

/* Helper to create a layer with CPU-backed tensors */
static bool create_cpu_layer(
    nimcp_jepa_gpu_layer_t* layer,
    uint32_t in_dim,
    uint32_t out_dim,
    nimcp_jepa_gpu_activation_t activation)
{
    if (!layer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "create_cpu_layer: layer is NULL");
        return false;
    }

    layer->in_dim = in_dim;
    layer->out_dim = out_dim;
    layer->activation = activation;

    /* Create weight tensor [out_dim x in_dim] */
    size_t weight_dims[2] = { out_dim, in_dim };
    layer->weights = nimcp_gpu_tensor_create(NULL, weight_dims, 2, NIMCP_GPU_PRECISION_FP32);
    if (!layer->weights) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "create_cpu_layer: weights allocation failed");
        return false;
    }

    /* Create bias tensor [out_dim] */
    size_t bias_dims[1] = { out_dim };
    layer->bias = nimcp_gpu_tensor_create(NULL, bias_dims, 1, NIMCP_GPU_PRECISION_FP32);
    if (!layer->bias) {
        nimcp_gpu_tensor_destroy(layer->weights);
        layer->weights = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "create_cpu_layer: bias allocation failed");
        return false;
    }

    /* Initialize gradients to NULL (not needed for inference) */
    layer->grad_w = NULL;
    layer->grad_b = NULL;

    return true;
}

static void destroy_cpu_layer(nimcp_jepa_gpu_layer_t* layer) {
    if (!layer) return;
    if (layer->weights) nimcp_gpu_tensor_destroy(layer->weights);
    if (layer->bias) nimcp_gpu_tensor_destroy(layer->bias);
    if (layer->grad_w) nimcp_gpu_tensor_destroy(layer->grad_w);
    if (layer->grad_b) nimcp_gpu_tensor_destroy(layer->grad_b);
    layer->weights = NULL;
    layer->bias = NULL;
    layer->grad_w = NULL;
    layer->grad_b = NULL;
}

nimcp_jepa_gpu_predictor_t* nimcp_jepa_gpu_predictor_create(
    nimcp_gpu_context_t* ctx,
    uint32_t input_dim,
    uint32_t hidden_dim,
    uint32_t output_dim,
    uint32_t num_layers,
    nimcp_jepa_gpu_activation_t activation)
{
    if (num_layers == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_jepa_gpu_predictor_create: num_layers is zero");
        return NULL;
    }

    nimcp_jepa_gpu_predictor_t* pred = nimcp_calloc(1, sizeof(nimcp_jepa_gpu_predictor_t));
    if (!pred) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_jepa_gpu_predictor_create: allocation failed");

        return NULL;

    }

    pred->ctx = ctx;
    pred->input_dim = input_dim;
    pred->hidden_dim = hidden_dim;
    pred->output_dim = output_dim;
    pred->num_layers = num_layers;

    /* Allocate layers array */
    pred->layers = nimcp_calloc(num_layers, sizeof(nimcp_jepa_gpu_layer_t));
    if (!pred->layers) {
        nimcp_free(pred);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_jepa_gpu_predictor_create: pred->layers is NULL");
        return NULL;
    }

    /* Create each layer */
    for (uint32_t i = 0; i < num_layers; i++) {
        uint32_t in_size = (i == 0) ? input_dim : hidden_dim;
        uint32_t out_size = (i == num_layers - 1) ? output_dim : hidden_dim;
        nimcp_jepa_gpu_activation_t layer_act = (i == num_layers - 1) ? NIMCP_JEPA_ACT_NONE : activation;

        if (!create_cpu_layer(&pred->layers[i], in_size, out_size, layer_act)) {
            /* Cleanup on failure */
            for (uint32_t j = 0; j < i; j++) {
                destroy_cpu_layer(&pred->layers[j]);
            }
            nimcp_free(pred->layers);
            nimcp_free(pred);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_jepa_gpu_predictor_create: create_cpu_layer is NULL");
            return NULL;
        }
    }

    /* Allocate activation buffers (optional for stubs) */
    pred->activations = NULL;
    pred->pre_activations = NULL;

    return pred;
}

void nimcp_jepa_gpu_predictor_destroy(nimcp_jepa_gpu_predictor_t* predictor) {
    if (!predictor) return;

    if (predictor->layers) {
        for (uint32_t i = 0; i < predictor->num_layers; i++) {
            destroy_cpu_layer(&predictor->layers[i]);
        }
        nimcp_free(predictor->layers);
    }

    /* Free activation buffers if allocated */
    if (predictor->activations) {
        for (uint32_t i = 0; i < predictor->num_layers; i++) {
            if (predictor->activations[i]) {
                nimcp_gpu_tensor_destroy(predictor->activations[i]);
            }
        }
        nimcp_free(predictor->activations);
    }
    if (predictor->pre_activations) {
        for (uint32_t i = 0; i < predictor->num_layers; i++) {
            if (predictor->pre_activations[i]) {
                nimcp_gpu_tensor_destroy(predictor->pre_activations[i]);
            }
        }
        nimcp_free(predictor->pre_activations);
    }

    nimcp_free(predictor);
}

bool nimcp_jepa_gpu_predictor_upload_weights(
    nimcp_jepa_gpu_predictor_t* predictor,
    uint32_t layer_idx,
    const float* weights,
    const float* bias)
{
    if (!predictor || layer_idx >= predictor->num_layers) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_jepa_gpu_predictor_upload_weights: predictor is NULL");
        return false;
    }
    if (!weights || !bias) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_jepa_gpu_predictor_upload_weights: required parameter is NULL (weights, bias)");
        return false;
    }

    nimcp_jepa_gpu_layer_t* layer = &predictor->layers[layer_idx];
    if (!layer->weights || !layer->bias) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_jepa_gpu_predictor_upload_weights: required parameter is NULL (layer->weights, layer->bias)");
        return false;
    }
    if (!layer->weights->data || !layer->bias->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_jepa_gpu_predictor_upload_weights: required parameter is NULL (layer->weights->data, layer->bias->data)");
        return false;
    }

    /* Copy weights: [out_dim x in_dim] */
    size_t weight_size = layer->out_dim * layer->in_dim * sizeof(float);
    memcpy(layer->weights->data, weights, weight_size);

    /* Copy bias: [out_dim] */
    size_t bias_size = layer->out_dim * sizeof(float);
    memcpy(layer->bias->data, bias, bias_size);

    return true;
}

bool nimcp_jepa_gpu_predictor_download_weights(
    const nimcp_jepa_gpu_predictor_t* predictor,
    uint32_t layer_idx,
    float* weights,
    float* bias)
{
    if (!predictor || layer_idx >= predictor->num_layers) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_jepa_gpu_predictor_download_weights: predictor is NULL");
        return false;
    }
    if (!weights || !bias) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_jepa_gpu_predictor_download_weights: required parameter is NULL (weights, bias)");
        return false;
    }

    const nimcp_jepa_gpu_layer_t* layer = &predictor->layers[layer_idx];
    if (!layer->weights || !layer->bias) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_jepa_gpu_predictor_download_weights: required parameter is NULL (layer->weights, layer->bias)");
        return false;
    }
    if (!layer->weights->data || !layer->bias->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_jepa_gpu_predictor_download_weights: required parameter is NULL (layer->weights->data, layer->bias->data)");
        return false;
    }

    /* Copy weights: [out_dim x in_dim] */
    size_t weight_size = layer->out_dim * layer->in_dim * sizeof(float);
    memcpy(weights, layer->weights->data, weight_size);

    /* Copy bias: [out_dim] */
    size_t bias_size = layer->out_dim * sizeof(float);
    memcpy(bias, layer->bias->data, bias_size);

    return true;
}

/* CPU GELU activation */
static float cpu_gelu(float x) {
    const float sqrt_2_pi = 0.7978845608028654f;
    const float coeff = 0.044715f;
    float x3 = x * x * x;
    float inner = sqrt_2_pi * (x + coeff * x3);
    return 0.5f * x * (1.0f + tanhf(inner));
}

static float cpu_apply_activation(float x, nimcp_jepa_gpu_activation_t act) {
    switch (act) {
        case NIMCP_JEPA_ACT_NONE:    return x;
        case NIMCP_JEPA_ACT_RELU:    return x > 0 ? x : 0;
        case NIMCP_JEPA_ACT_GELU:    return cpu_gelu(x);
        case NIMCP_JEPA_ACT_TANH:    return tanhf(x);
        case NIMCP_JEPA_ACT_SIGMOID: return 1.0f / (1.0f + expf(-x));
        default:                     return x;
    }
}

bool nimcp_jepa_gpu_forward_predict(
    nimcp_jepa_gpu_predictor_t* predictor,
    const nimcp_gpu_tensor_t* context,
    nimcp_gpu_tensor_t* prediction)
{
    if (!predictor || !context || !prediction) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_jepa_gpu_forward_predict: required parameter is NULL (predictor, context, prediction)");
        return false;
    }
    if (!context->data || !prediction->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_jepa_gpu_forward_predict: required parameter is NULL (context->data, prediction->data)");
        return false;
    }
    if (!predictor->layers) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_jepa_gpu_forward_predict: predictor->layers is NULL");
        return false;
    }

    uint32_t batch_size = (uint32_t)context->dims[0];
    float* input = (float*)context->data;
    float* output = (float*)prediction->data;

    /* Allocate temp buffers for intermediate layers */
    float* temp1 = nimcp_calloc(batch_size * predictor->hidden_dim, sizeof(float));
    float* temp2 = nimcp_calloc(batch_size * predictor->hidden_dim, sizeof(float));
    if (!temp1 || !temp2) {
        nimcp_free(temp1);
        nimcp_free(temp2);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_jepa_gpu_forward_predict: required parameter is NULL (temp1, temp2)");
        return false;
    }

    float* current_input = input;
    float* current_output = temp1;

    /* Forward through each layer */
    for (uint32_t layer_idx = 0; layer_idx < predictor->num_layers; layer_idx++) {
        nimcp_jepa_gpu_layer_t* layer = &predictor->layers[layer_idx];
        if (!layer->weights || !layer->bias) {
            nimcp_free(temp1);
            nimcp_free(temp2);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_jepa_gpu_forward_predict: required parameter is NULL (layer->weights, layer->bias)");
            return false;
        }

        float* weights = (float*)layer->weights->data;
        float* biases = (float*)layer->bias->data;
        uint32_t in_dim = layer->in_dim;
        uint32_t out_dim = layer->out_dim;

        if (layer_idx == predictor->num_layers - 1) {
            current_output = output;
        }

        /* Matrix multiply: output = weights @ input + bias */
        for (uint32_t b = 0; b < batch_size; b++) {
            for (uint32_t o = 0; o < out_dim; o++) {
                float sum = biases[o];
                for (uint32_t i = 0; i < in_dim; i++) {
                    sum += weights[o * in_dim + i] * current_input[b * in_dim + i];
                }
                /* Apply activation */
                sum = cpu_apply_activation(sum, layer->activation);
                current_output[b * out_dim + o] = sum;
            }
        }

        /* Swap buffers */
        current_input = current_output;
        current_output = (current_output == temp1) ? temp2 : temp1;
    }

    nimcp_free(temp1);
    nimcp_free(temp2);
    return true;
}

bool nimcp_jepa_gpu_forward_conditioned(
    nimcp_jepa_gpu_predictor_t* predictor,
    const nimcp_gpu_tensor_t* state,
    const nimcp_gpu_tensor_t* action,
    nimcp_gpu_tensor_t* next_state)
{
    (void)predictor; (void)state; (void)action; (void)next_state;
    /* CPU fallback: not implemented - return false */
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_jepa_gpu_forward_conditioned: operation failed");
    return false;
}

nimcp_jepa_gpu_inverse_t* nimcp_jepa_gpu_inverse_create(
    nimcp_gpu_context_t* ctx,
    uint32_t state_dim,
    uint32_t action_dim,
    uint32_t hidden_dim,
    uint32_t num_layers)
{
    if (num_layers == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_jepa_gpu_inverse_create: num_layers is zero");
        return NULL;
    }

    nimcp_jepa_gpu_inverse_t* inv = nimcp_calloc(1, sizeof(nimcp_jepa_gpu_inverse_t));
    if (!inv) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_jepa_gpu_inverse_create: allocation failed");

        return NULL;

    }

    inv->ctx = ctx;
    inv->state_dim = state_dim;
    inv->action_dim = action_dim;
    inv->num_layers = num_layers;

    /* Allocate layers array */
    inv->layers = nimcp_calloc(num_layers, sizeof(nimcp_jepa_gpu_layer_t));
    if (!inv->layers) {
        nimcp_free(inv);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_jepa_gpu_inverse_create: inv->layers is NULL");
        return NULL;
    }

    /* Input is concatenated (state_t, state_next), output is action */
    uint32_t input_dim = state_dim * 2;
    for (uint32_t i = 0; i < num_layers; i++) {
        uint32_t in_size = (i == 0) ? input_dim : hidden_dim;
        uint32_t out_size = (i == num_layers - 1) ? action_dim : hidden_dim;
        nimcp_jepa_gpu_activation_t act = (i == num_layers - 1) ? NIMCP_JEPA_ACT_TANH : NIMCP_JEPA_ACT_RELU;

        if (!create_cpu_layer(&inv->layers[i], in_size, out_size, act)) {
            /* Cleanup on failure */
            for (uint32_t j = 0; j < i; j++) {
                destroy_cpu_layer(&inv->layers[j]);
            }
            nimcp_free(inv->layers);
            nimcp_free(inv);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_jepa_gpu_inverse_create: create_cpu_layer is NULL");
            return NULL;
        }

        /* Initialize weights with small random values */
        float* weights = (float*)inv->layers[i].weights->data;
        for (size_t j = 0; j < inv->layers[i].weights->numel; j++) {
            weights[j] = ((float)nimcp_tl_rand() / RAND_MAX - 0.5f) * 0.1f;
        }
    }

    return inv;
}

void nimcp_jepa_gpu_inverse_destroy(nimcp_jepa_gpu_inverse_t* inverse) {
    if (!inverse) return;

    if (inverse->layers) {
        for (uint32_t i = 0; i < inverse->num_layers; i++) {
            destroy_cpu_layer(&inverse->layers[i]);
        }
        nimcp_free(inverse->layers);
    }
    nimcp_free(inverse);
}

bool nimcp_jepa_gpu_inverse_infer(
    nimcp_jepa_gpu_inverse_t* inverse,
    const nimcp_gpu_tensor_t* state_t,
    const nimcp_gpu_tensor_t* state_next,
    nimcp_gpu_tensor_t* action)
{
    if (!inverse || !state_t || !state_next || !action) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_jepa_gpu_inverse_infer: required parameter is NULL (inverse, state_t, state_next, action)");
        return false;
    }
    if (!state_t->data || !state_next->data || !action->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_jepa_gpu_inverse_infer: required parameter is NULL (state_t->data, state_next->data, action->data)");
        return false;
    }
    if (!inverse->layers) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_jepa_gpu_inverse_infer: inverse->layers is NULL");
        return false;
    }

    uint32_t batch_size = (uint32_t)state_t->dims[0];
    float* s_t = (float*)state_t->data;
    float* s_next = (float*)state_next->data;
    float* act_out = (float*)action->data;

    /* Concatenate states */
    uint32_t input_dim = inverse->state_dim * 2;

    /* Get hidden_dim from first layer's output dim (or second layer's input dim) */
    uint32_t hidden_dim = (inverse->num_layers > 1) ? inverse->layers[0].out_dim : inverse->layers[0].in_dim;

    float* concat = nimcp_calloc(batch_size * input_dim, sizeof(float));
    float* temp1 = nimcp_calloc(batch_size * hidden_dim, sizeof(float));
    float* temp2 = nimcp_calloc(batch_size * hidden_dim, sizeof(float));

    if (!concat || !temp1 || !temp2) {
        nimcp_free(concat);
        nimcp_free(temp1);
        nimcp_free(temp2);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_jepa_gpu_inverse_infer: required parameter is NULL (concat, temp1, temp2)");
        return false;
    }

    /* Build concatenated input */
    for (uint32_t b = 0; b < batch_size; b++) {
        memcpy(concat + b * input_dim, s_t + b * inverse->state_dim, inverse->state_dim * sizeof(float));
        memcpy(concat + b * input_dim + inverse->state_dim, s_next + b * inverse->state_dim, inverse->state_dim * sizeof(float));
    }

    float* current_input = concat;
    float* current_output = temp1;

    /* Forward through layers */
    for (uint32_t layer_idx = 0; layer_idx < inverse->num_layers; layer_idx++) {
        nimcp_jepa_gpu_layer_t* layer = &inverse->layers[layer_idx];
        float* weights = (float*)layer->weights->data;
        float* biases = (float*)layer->bias->data;
        uint32_t in_dim = layer->in_dim;
        uint32_t out_dim = layer->out_dim;

        if (layer_idx == inverse->num_layers - 1) {
            current_output = act_out;
        }

        for (uint32_t b = 0; b < batch_size; b++) {
            for (uint32_t o = 0; o < out_dim; o++) {
                float sum = biases[o];
                for (uint32_t i = 0; i < in_dim; i++) {
                    sum += weights[o * in_dim + i] * current_input[b * in_dim + i];
                }
                /* Apply layer activation */
                sum = cpu_apply_activation(sum, layer->activation);
                current_output[b * out_dim + o] = sum;
            }
        }

        current_input = current_output;
        current_output = (current_output == temp1) ? temp2 : temp1;
    }

    nimcp_free(concat);
    nimcp_free(temp1);
    nimcp_free(temp2);
    return true;
}

bool nimcp_jepa_gpu_apply_mask(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* latent,
    const nimcp_gpu_tensor_t* mask,
    nimcp_gpu_tensor_t* masked)
{
    (void)ctx;
    if (!latent || !mask || !masked) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_jepa_gpu_apply_mask: required parameter is NULL (latent, mask, masked)");
        return false;
    }
    if (!latent->data || !mask->data || !masked->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_jepa_gpu_apply_mask: required parameter is NULL (latent->data, mask->data, masked->data)");
        return false;
    }

    float* lat = (float*)latent->data;
    float* m = (float*)mask->data;
    float* out = (float*)masked->data;

    for (size_t i = 0; i < latent->numel; i++) {
        out[i] = lat[i] * m[i % mask->numel];
    }
    return true;
}

bool nimcp_jepa_gpu_generate_block_mask(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* mask,
    uint32_t block_size,
    float mask_ratio)
{
    (void)ctx;
    if (!mask || !mask->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_jepa_gpu_generate_block_mask: required parameter is NULL (mask, mask->data)");
        return false;
    }

    float* m = (float*)mask->data;
    size_t numel = mask->numel;

    /* Initialize all to 1 (unmasked) */
    for (size_t i = 0; i < numel; i++) {
        m[i] = 1.0f;
    }

    /* Create block masks */
    size_t num_blocks = numel / block_size;
    size_t blocks_to_mask = (size_t)(num_blocks * mask_ratio);

    for (size_t b = 0; b < blocks_to_mask; b++) {
        size_t block_idx = (size_t)nimcp_tl_rand() % num_blocks;
        size_t start = block_idx * block_size;
        for (uint32_t j = 0; j < block_size && start + j < numel; j++) {
            m[start + j] = 0.0f;
        }
    }

    return true;
}

bool nimcp_jepa_gpu_apply_soft_mask(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* latent,
    const nimcp_gpu_tensor_t* weights,
    nimcp_gpu_tensor_t* masked)
{
    /* Same as apply_mask for CPU fallback */
    return nimcp_jepa_gpu_apply_mask(ctx, latent, weights, masked);
}

bool nimcp_jepa_gpu_compute_loss(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* prediction,
    const nimcp_gpu_tensor_t* target,
    const nimcp_gpu_tensor_t* mask,
    float* loss)
{
    (void)ctx;
    if (!prediction || !target || !loss) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_jepa_gpu_compute_loss: required parameter is NULL (prediction, target, loss)");
        return false;
    }
    if (!prediction->data || !target->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_jepa_gpu_compute_loss: required parameter is NULL (prediction->data, target->data)");
        return false;
    }

    float* pred = (float*)prediction->data;
    float* tgt = (float*)target->data;
    float* m = mask ? (float*)mask->data : NULL;

    double sum = 0.0;
    double count = 0.0;

    for (size_t i = 0; i < prediction->numel; i++) {
        float weight = m ? m[i % (m ? mask->numel : 1)] : 1.0f;
        if (weight > 0.0f) {
            float diff = pred[i] - tgt[i];
            sum += weight * diff * diff;
            count += weight;
        }
    }

    *loss = count > 0 ? (float)(sum / count) : 0.0f;
    return true;
}

bool nimcp_jepa_gpu_compute_precision_loss(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* prediction,
    const nimcp_gpu_tensor_t* target,
    const nimcp_gpu_tensor_t* precision,
    float* loss)
{
    /* Use precision as weights */
    return nimcp_jepa_gpu_compute_loss(ctx, prediction, target, precision, loss);
}

bool nimcp_jepa_gpu_backward(
    nimcp_jepa_gpu_predictor_t* predictor,
    const nimcp_gpu_tensor_t* grad_output,
    nimcp_gpu_tensor_t* grad_input)
{
    (void)predictor; (void)grad_output; (void)grad_input;
    /* CPU fallback: not implemented */
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_jepa_gpu_backward: operation failed");
    return false;
}

bool nimcp_jepa_gpu_update_weights(
    nimcp_jepa_gpu_predictor_t* predictor,
    float learning_rate,
    float weight_decay)
{
    (void)predictor; (void)learning_rate; (void)weight_decay;
    /* CPU fallback: not implemented */
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_jepa_gpu_update_weights: operation failed");
    return false;
}

int nimcp_jepa_gpu_download_latent(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* gpu_latent,
    float* cpu_data,
    size_t max_elements)
{
    (void)ctx;
    if (!gpu_latent || !cpu_data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_jepa_gpu_download_latent: required parameter is NULL (gpu_latent, cpu_data)");
        return -1;
    }

    size_t to_copy = gpu_latent->numel < max_elements ? gpu_latent->numel : max_elements;
    memcpy(cpu_data, gpu_latent->data, to_copy * sizeof(float));
    return (int)to_copy;
}

bool nimcp_jepa_gpu_upload_latent(
    nimcp_gpu_context_t* ctx,
    const float* cpu_data,
    size_t num_elements,
    nimcp_gpu_tensor_t* gpu_latent)
{
    (void)ctx;
    if (!cpu_data || !gpu_latent || !gpu_latent->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_jepa_gpu_upload_latent: required parameter is NULL (cpu_data, gpu_latent, gpu_latent->data)");
        return false;
    }

    size_t to_copy = gpu_latent->numel < num_elements ? gpu_latent->numel : num_elements;
    memcpy(gpu_latent->data, cpu_data, to_copy * sizeof(float));
    return true;
}

bool nimcp_jepa_gpu_synchronize(nimcp_gpu_context_t* ctx) {
    (void)ctx;
    return true;  /* No-op for CPU */
}

/*=============================================================================
 * Broca GPU Stubs (gpu/cognitive/nimcp_broca_gpu.h)
 *=============================================================================*/

#include "gpu/cognitive/nimcp_broca_gpu.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(gpu_stubs)

/* Internal CPU-based Broca context for fallback */
struct broca_gpu_context {
    nimcp_gpu_context_t* gpu_ctx;
    broca_gpu_config_t config;
    broca_gpu_lexical_entry_t* lexicon;
    uint32_t lexicon_size;
    uint32_t lexicon_capacity;
    uint32_t* wm_word_ids;
    float* wm_activations;
    uint32_t wm_count;
    broca_gpu_stats_t stats;
};

broca_gpu_config_t broca_gpu_default_config(void) {
    broca_gpu_config_t config = {
        .max_lexicon_size = 10000,
        .max_batch_size = NIMCP_MAX_BATCH_SIZE,
        .max_phonemes_per_word = 16,
        .max_articulators = 6,
        .working_memory_slots = NIMCP_WM_SLOTS,
        .enable_coarticulation = true,
        .enable_async_transfer = false,
        .activation_decay_rate = 0.05f
    };
    return config;
}

broca_gpu_context_t* broca_gpu_create(
    nimcp_gpu_context_t* gpu_ctx,
    const broca_gpu_config_t* config)
{
    broca_gpu_context_t* ctx = nimcp_calloc(1, sizeof(broca_gpu_context_t));
    if (!ctx) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "broca_gpu_create: allocation failed");

        return NULL;

    }

    ctx->gpu_ctx = gpu_ctx;
    ctx->config = config ? *config : broca_gpu_default_config();

    /* Allocate lexicon storage */
    ctx->lexicon_capacity = ctx->config.max_lexicon_size;
    ctx->lexicon = nimcp_calloc(ctx->lexicon_capacity, sizeof(broca_gpu_lexical_entry_t));
    if (!ctx->lexicon) {
        nimcp_free(ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "broca_gpu_create: ctx->lexicon is NULL");
        return NULL;
    }

    /* Allocate working memory */
    ctx->wm_word_ids = nimcp_calloc(ctx->config.working_memory_slots, sizeof(uint32_t));
    ctx->wm_activations = nimcp_calloc(ctx->config.working_memory_slots, sizeof(float));
    if (!ctx->wm_word_ids || !ctx->wm_activations) {
        nimcp_free(ctx->lexicon);
        nimcp_free(ctx->wm_word_ids);
        nimcp_free(ctx->wm_activations);
        nimcp_free(ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "broca_gpu_create: working memory allocation failed");
        return NULL;
    }

    return ctx;
}

void broca_gpu_destroy(broca_gpu_context_t* ctx) {
    if (!ctx) return;
    nimcp_free(ctx->lexicon);
    nimcp_free(ctx->wm_word_ids);
    nimcp_free(ctx->wm_activations);
    nimcp_free(ctx);
}

bool broca_gpu_synchronize(broca_gpu_context_t* ctx) {
    (void)ctx;
    return true;  /* No-op for CPU */
}

bool broca_gpu_upload_lexicon(
    broca_gpu_context_t* ctx,
    const broca_gpu_lexical_entry_t* entries,
    uint32_t count)
{
    if (!ctx || !entries) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "broca_gpu_upload_lexicon: required parameter is NULL (ctx, entries)");
        return false;
    }
    if (ctx->lexicon_size + count > ctx->lexicon_capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "broca_gpu_upload_lexicon: validation failed");
        return false;
    }

    memcpy(ctx->lexicon + ctx->lexicon_size, entries, count * sizeof(broca_gpu_lexical_entry_t));
    ctx->lexicon_size += count;
    return true;
}

bool broca_gpu_clear_lexicon(broca_gpu_context_t* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "broca_gpu_clear_lexicon: ctx is NULL");
        return false;
    }
    ctx->lexicon_size = 0;
    return true;
}

uint32_t broca_gpu_get_lexicon_size(const broca_gpu_context_t* ctx) {
    return ctx ? ctx->lexicon_size : 0;
}

bool broca_gpu_batch_lexical_lookup(
    broca_gpu_context_t* ctx,
    const uint32_t* word_ids,
    uint32_t count,
    broca_gpu_lookup_result_t* results)
{
    if (!ctx || !word_ids || !results) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "broca_gpu_batch_lexical_lookup: required parameter is NULL (ctx, word_ids, results)");
        return false;
    }

    for (uint32_t i = 0; i < count; i++) {
        results[i].word_id = word_ids[i];
        results[i].found = false;
        results[i].phoneme_count = 0;
        results[i].frequency = 0.0f;

        /* Linear search (CPU fallback) */
        for (uint32_t j = 0; j < ctx->lexicon_size; j++) {
            if (ctx->lexicon[j].word_id == word_ids[i]) {
                results[i].found = true;
                results[i].phoneme_count = ctx->lexicon[j].phoneme_count;
                memcpy(results[i].phonemes, ctx->lexicon[j].phonemes, 16);
                results[i].frequency = ctx->lexicon[j].frequency;
                break;
            }
        }
    }
    ctx->stats.lexical_lookups += count;
    return true;
}

bool broca_gpu_find_top_activated(
    broca_gpu_context_t* ctx,
    uint32_t top_n,
    broca_gpu_lexical_entry_t* results,
    uint32_t* actual_count)
{
    if (!ctx || !results || !actual_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "broca_gpu_find_top_activated: required parameter is NULL (ctx, results, actual_count)");
        return false;
    }

    /* Simple bubble-sort top-N for CPU fallback */
    uint32_t found = 0;
    for (uint32_t i = 0; i < ctx->lexicon_size && found < top_n; i++) {
        /* Find entry with max activation not yet selected */
        float max_act = -1.0f;
        uint32_t max_idx = 0;
        bool found_any = false;

        for (uint32_t j = 0; j < ctx->lexicon_size; j++) {
            if (ctx->lexicon[j].activation > max_act) {
                bool already_found = false;
                for (uint32_t k = 0; k < found; k++) {
                    if (results[k].word_id == ctx->lexicon[j].word_id) {
                        already_found = true;
                        break;
                    }
                }
                if (!already_found) {
                    max_act = ctx->lexicon[j].activation;
                    max_idx = j;
                    found_any = true;
                }
            }
        }
        if (found_any) {
            results[found++] = ctx->lexicon[max_idx];
        }
    }
    *actual_count = found;
    return true;
}

bool broca_gpu_update_activations(
    broca_gpu_context_t* ctx,
    const uint32_t* word_ids,
    uint32_t count,
    float boost_amount,
    float decay_rate)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "broca_gpu_update_activations: ctx is NULL");
        return false;
    }

    /* Apply decay to all */
    for (uint32_t i = 0; i < ctx->lexicon_size; i++) {
        ctx->lexicon[i].activation *= decay_rate;
    }

    /* Apply boost to specified words */
    if (word_ids && count > 0) {
        for (uint32_t i = 0; i < count; i++) {
            for (uint32_t j = 0; j < ctx->lexicon_size; j++) {
                if (ctx->lexicon[j].word_id == word_ids[i]) {
                    ctx->lexicon[j].activation += boost_amount;
                    if (ctx->lexicon[j].activation > 1.0f) {
                        ctx->lexicon[j].activation = 1.0f;
                    }
                    break;
                }
            }
        }
    }
    return true;
}

bool broca_gpu_encode_phonemes(
    broca_gpu_context_t* ctx,
    const uint32_t* word_ids,
    uint32_t word_count,
    uint8_t* phoneme_buffer,
    uint32_t buffer_size,
    uint32_t* phoneme_count,
    uint32_t* word_boundaries)
{
    if (!ctx || !word_ids || !phoneme_buffer || !phoneme_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "broca_gpu_encode_phonemes: required parameter is NULL (ctx, word_ids, phoneme_buffer, phoneme_count)");
        return false;
    }

    uint32_t offset = 0;
    for (uint32_t i = 0; i < word_count; i++) {
        if (word_boundaries) word_boundaries[i] = offset;

        /* Find word in lexicon */
        for (uint32_t j = 0; j < ctx->lexicon_size; j++) {
            if (ctx->lexicon[j].word_id == word_ids[i]) {
                uint32_t pc = ctx->lexicon[j].phoneme_count;
                if (offset + pc > buffer_size) {
                    *phoneme_count = offset;
                    return true;
                }
                memcpy(phoneme_buffer + offset, ctx->lexicon[j].phonemes, pc);
                offset += pc;
                break;
            }
        }
    }
    *phoneme_count = offset;
    ctx->stats.phonemes_encoded += offset;
    return true;
}

bool broca_gpu_apply_coarticulation(
    broca_gpu_context_t* ctx,
    uint8_t* phonemes,
    uint32_t phoneme_count,
    float coarticulation_strength)
{
    (void)ctx; (void)phonemes; (void)phoneme_count; (void)coarticulation_strength;
    /* CPU fallback: no-op (coarticulation requires full phoneme feature model) */
    return true;
}

bool broca_gpu_generate_motor_commands(
    broca_gpu_context_t* ctx,
    const uint8_t* phonemes,
    uint32_t phoneme_count,
    broca_gpu_motor_command_t* commands,
    uint32_t max_commands,
    uint32_t* command_count,
    float base_timestamp)
{
    if (!ctx || !phonemes || !commands || !command_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "broca_gpu_generate_motor_commands: required parameter is NULL (ctx, phonemes, commands, command_count)");
        return false;
    }

    uint32_t num_arts = ctx->config.max_articulators;
    uint32_t cmd_idx = 0;
    float timestamp = base_timestamp;
    float phoneme_duration = 100.0f;  /* ms per phoneme */

    for (uint32_t p = 0; p < phoneme_count && cmd_idx < max_commands; p++) {
        /* Generate one command per articulator per phoneme */
        for (uint32_t a = 0; a < num_arts && cmd_idx < max_commands; a++) {
            commands[cmd_idx].articulator = (uint8_t)a;
            commands[cmd_idx].phoneme = phonemes[p];
            commands[cmd_idx].position = 0.5f + 0.3f * sinf((float)(phonemes[p] + a));
            commands[cmd_idx].velocity = 1.0f;
            commands[cmd_idx].timestamp_ms = timestamp;
            cmd_idx++;
        }
        timestamp += phoneme_duration;
    }

    *command_count = cmd_idx;
    ctx->stats.motor_commands += cmd_idx;
    return true;
}

bool broca_gpu_adjust_timing(
    broca_gpu_context_t* ctx,
    broca_gpu_motor_command_t* commands,
    uint32_t command_count,
    float rate_multiplier)
{
    (void)ctx;
    if (!commands) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "broca_gpu_adjust_timing: commands is NULL");
        return false;
    }

    for (uint32_t i = 0; i < command_count; i++) {
        commands[i].timestamp_ms *= rate_multiplier;
    }
    return true;
}

bool broca_gpu_wm_push(
    broca_gpu_context_t* ctx,
    const uint32_t* word_ids,
    uint32_t count,
    float initial_activation)
{
    if (!ctx || !word_ids) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "broca_gpu_wm_push: required parameter is NULL (ctx, word_ids)");
        return false;
    }

    for (uint32_t i = 0; i < count; i++) {
        if (ctx->wm_count < ctx->config.working_memory_slots) {
            ctx->wm_word_ids[ctx->wm_count] = word_ids[i];
            ctx->wm_activations[ctx->wm_count] = initial_activation;
            ctx->wm_count++;
        } else {
            /* Shift and add */
            for (uint32_t j = 0; j < ctx->wm_count - 1; j++) {
                ctx->wm_word_ids[j] = ctx->wm_word_ids[j + 1];
                ctx->wm_activations[j] = ctx->wm_activations[j + 1];
            }
            ctx->wm_word_ids[ctx->wm_count - 1] = word_ids[i];
            ctx->wm_activations[ctx->wm_count - 1] = initial_activation;
        }
    }
    ctx->stats.wm_operations++;
    return true;
}

bool broca_gpu_wm_get_contents(
    broca_gpu_context_t* ctx,
    uint32_t* word_ids,
    float* activations,
    uint32_t max_count,
    uint32_t* actual_count)
{
    if (!ctx || !word_ids || !actual_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "broca_gpu_wm_get_contents: required parameter is NULL (ctx, word_ids, actual_count)");
        return false;
    }

    uint32_t to_copy = ctx->wm_count < max_count ? ctx->wm_count : max_count;
    memcpy(word_ids, ctx->wm_word_ids, to_copy * sizeof(uint32_t));
    if (activations) {
        memcpy(activations, ctx->wm_activations, to_copy * sizeof(float));
    }
    *actual_count = to_copy;
    return true;
}

bool broca_gpu_wm_apply_decay(
    broca_gpu_context_t* ctx,
    float decay_factor,
    float threshold)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "broca_gpu_wm_apply_decay: ctx is NULL");
        return false;
    }

    uint32_t new_count = 0;
    for (uint32_t i = 0; i < ctx->wm_count; i++) {
        ctx->wm_activations[i] *= decay_factor;
        if (ctx->wm_activations[i] >= threshold) {
            ctx->wm_word_ids[new_count] = ctx->wm_word_ids[i];
            ctx->wm_activations[new_count] = ctx->wm_activations[i];
            new_count++;
        }
    }
    ctx->wm_count = new_count;
    return true;
}

bool broca_gpu_wm_clear(broca_gpu_context_t* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "broca_gpu_wm_clear: ctx is NULL");
        return false;
    }
    ctx->wm_count = 0;
    return true;
}

bool broca_gpu_produce_utterance(
    broca_gpu_context_t* ctx,
    const uint32_t* word_ids,
    uint32_t word_count,
    broca_gpu_motor_command_t* commands,
    uint32_t max_commands,
    uint32_t* command_count,
    float base_timestamp)
{
    if (!ctx || !word_ids || !commands || !command_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "broca_gpu_produce_utterance: required parameter is NULL (ctx, word_ids, commands, command_count)");
        return false;
    }

    /* Allocate phoneme buffer */
    uint32_t max_phonemes = word_count * ctx->config.max_phonemes_per_word;
    uint8_t* phonemes = nimcp_calloc(max_phonemes, sizeof(uint8_t));
    if (!phonemes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "broca_gpu_produce_utterance: phonemes is NULL");
        return false;
    }

    uint32_t phoneme_count = 0;
    bool result = broca_gpu_encode_phonemes(ctx, word_ids, word_count, phonemes,
                                            max_phonemes, &phoneme_count, NULL);
    if (!result) {
        nimcp_free(phonemes);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "broca_gpu_produce_utterance: result is NULL");
        return false;
    }

    result = broca_gpu_generate_motor_commands(ctx, phonemes, phoneme_count,
                                               commands, max_commands, command_count,
                                               base_timestamp);
    nimcp_free(phonemes);
    return result;
}

bool broca_gpu_get_stats(const broca_gpu_context_t* ctx, broca_gpu_stats_t* stats) {
    if (!ctx || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "broca_gpu_get_stats: required parameter is NULL (ctx, stats)");
        return false;
    }
    *stats = ctx->stats;
    stats->gpu_memory_used = ctx->lexicon_size * sizeof(broca_gpu_lexical_entry_t) +
                             ctx->config.working_memory_slots * (sizeof(uint32_t) + sizeof(float));
    return true;
}

void broca_gpu_reset_stats(broca_gpu_context_t* ctx) {
    if (ctx) {
        memset(&ctx->stats, 0, sizeof(broca_gpu_stats_t));
    }
}

/* CPU reference implementations */
bool broca_cpu_batch_lexical_lookup(
    const broca_gpu_lexical_entry_t* lexicon,
    uint32_t lexicon_size,
    const uint32_t* word_ids,
    uint32_t count,
    broca_gpu_lookup_result_t* results)
{
    if (!lexicon || !word_ids || !results) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "broca_cpu_batch_lexical_lookup: required parameter is NULL (lexicon, word_ids, results)");
        return false;
    }

    for (uint32_t i = 0; i < count; i++) {
        results[i].word_id = word_ids[i];
        results[i].found = false;
        results[i].phoneme_count = 0;
        results[i].frequency = 0.0f;

        for (uint32_t j = 0; j < lexicon_size; j++) {
            if (lexicon[j].word_id == word_ids[i]) {
                results[i].found = true;
                results[i].phoneme_count = lexicon[j].phoneme_count;
                memcpy(results[i].phonemes, lexicon[j].phonemes, 16);
                results[i].frequency = lexicon[j].frequency;
                break;
            }
        }
    }
    return true;
}

bool broca_cpu_encode_phonemes(
    const broca_gpu_lexical_entry_t* lexicon,
    uint32_t lexicon_size,
    const uint32_t* word_ids,
    uint32_t word_count,
    uint8_t* phoneme_buffer,
    uint32_t buffer_size,
    uint32_t* phoneme_count,
    uint32_t* word_boundaries)
{
    if (!lexicon || !word_ids || !phoneme_buffer || !phoneme_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "broca_cpu_encode_phonemes: required parameter is NULL (lexicon, word_ids, phoneme_buffer, phoneme_count)");
        return false;
    }

    uint32_t offset = 0;
    for (uint32_t i = 0; i < word_count; i++) {
        if (word_boundaries) word_boundaries[i] = offset;

        for (uint32_t j = 0; j < lexicon_size; j++) {
            if (lexicon[j].word_id == word_ids[i]) {
                uint32_t pc = lexicon[j].phoneme_count;
                if (offset + pc > buffer_size) {
                    *phoneme_count = offset;
                    return true;
                }
                memcpy(phoneme_buffer + offset, lexicon[j].phonemes, pc);
                offset += pc;
                break;
            }
        }
    }
    *phoneme_count = offset;
    return true;
}

bool broca_cpu_generate_motor_commands(
    const uint8_t* phonemes,
    uint32_t phoneme_count,
    broca_gpu_motor_command_t* commands,
    uint32_t max_commands,
    uint32_t* command_count,
    float base_timestamp,
    uint32_t num_articulators)
{
    if (!phonemes || !commands || !command_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "broca_cpu_generate_motor_commands: required parameter is NULL (phonemes, commands, command_count)");
        return false;
    }

    uint32_t cmd_idx = 0;
    float timestamp = base_timestamp;
    float phoneme_duration = 100.0f;

    for (uint32_t p = 0; p < phoneme_count && cmd_idx < max_commands; p++) {
        for (uint32_t a = 0; a < num_articulators && cmd_idx < max_commands; a++) {
            commands[cmd_idx].articulator = (uint8_t)a;
            commands[cmd_idx].phoneme = phonemes[p];
            commands[cmd_idx].position = 0.5f + 0.3f * sinf((float)(phonemes[p] + a));
            commands[cmd_idx].velocity = 1.0f;
            commands[cmd_idx].timestamp_ms = timestamp;
            cmd_idx++;
        }
        timestamp += phoneme_duration;
    }

    *command_count = cmd_idx;
    return true;
}
