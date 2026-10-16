/**
 * @file nimcp_native_language.h
 * @brief Brain-native language production — replaces external LLM dependency
 *
 * Generates text directly from brain neural embeddings via learned vocabulary,
 * projection matrices, and autoregressive decoding with nucleus sampling.
 */
#ifndef NIMCP_NATIVE_LANGUAGE_H
#define NIMCP_NATIVE_LANGUAGE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t id;
    char text[64];
    float* embedding;
    uint32_t embed_dim;
    float frequency;
    uint32_t co_occurrences;
} nimcp_token_t;

typedef struct {
    nimcp_token_t* tokens;
    uint32_t vocab_size;
    uint32_t max_vocab;
    uint32_t embed_dim;
    uint32_t bos_id, eos_id, unk_id, pad_id;
} nimcp_vocabulary_t;

typedef struct {
    float** buffer;
    uint32_t capacity;
    uint32_t length;
    uint32_t head;
    uint32_t embed_dim;
} nimcp_phonological_loop_t;

typedef struct {
    uint32_t max_vocab_size;
    uint32_t embed_dim;
    uint32_t max_seq_length;
    float temperature;
    float top_p;
    float repetition_penalty;
    bool learn_vocabulary;
    float min_token_frequency;
} nimcp_language_config_t;

typedef struct nimcp_native_language nimcp_native_language_t;

/* Lifecycle */
nimcp_native_language_t* nimcp_native_language_create(const nimcp_language_config_t* config);
void nimcp_native_language_destroy(nimcp_native_language_t* lang);

/* Vocabulary */
int nimcp_language_load_vocabulary(nimcp_native_language_t* lang, const char* filepath);
int nimcp_language_save_vocabulary(const nimcp_native_language_t* lang, const char* filepath);
int nimcp_language_learn_token(nimcp_native_language_t* lang, const char* text,
    const float* context_embedding, uint32_t embed_dim);
uint32_t nimcp_language_get_vocab_size(const nimcp_native_language_t* lang);

/* Production */
int nimcp_language_generate(nimcp_native_language_t* lang,
    const float* brain_embedding, uint32_t embed_dim,
    char* output_text, uint32_t max_text_length);

/* Comprehension */
int nimcp_language_encode(nimcp_native_language_t* lang,
    const char* text, float* embedding, uint32_t max_dim);

/* Training */
int nimcp_language_train_step(nimcp_native_language_t* lang,
    const float* brain_embedding, uint32_t embed_dim,
    const char* target_text, float learning_rate);

/* Phonological loop */
nimcp_phonological_loop_t* nimcp_phonological_loop_create(uint32_t capacity, uint32_t embed_dim);
void nimcp_phonological_loop_destroy(nimcp_phonological_loop_t* loop);
int nimcp_phonological_loop_push(nimcp_phonological_loop_t* loop, const float* embedding);
void nimcp_phonological_loop_reset(nimcp_phonological_loop_t* loop);

nimcp_language_config_t nimcp_language_config_default(void);

#ifdef __cplusplus
}
#endif
#endif
