/**
 * @file nimcp_tokenizer.h
 * @brief Brain-native tokenizer — learns vocabulary from neural activation patterns
 */
#ifndef NIMCP_BRAIN_TOKENIZER_H
#define NIMCP_BRAIN_TOKENIZER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NIMCP_TOKEN_SPECIAL = 0,
    NIMCP_TOKEN_CHARACTER,
    NIMCP_TOKEN_SUBWORD,
    NIMCP_TOKEN_WORD,
    NIMCP_TOKEN_PHRASE,
} nimcp_token_type_t;

typedef struct {
    uint32_t id;
    char text[128];
    nimcp_token_type_t type;
    float frequency;
    float* embedding;
    uint32_t embed_dim;
    uint32_t merge_count;
} nimcp_token_entry_t;

typedef struct {
    uint32_t left_id;
    uint32_t right_id;
    uint32_t merged_id;
    float score;
} nimcp_merge_rule_t;

typedef struct {
    uint32_t max_vocab_size;
    uint32_t min_frequency;
    uint32_t embed_dim;
    float merge_threshold;
    bool learn_from_brain;
    bool enable_subword;
    bool enable_phrase;
    uint32_t max_token_length;
} nimcp_tokenizer_config_t;

typedef struct nimcp_tokenizer nimcp_tokenizer_t;

nimcp_tokenizer_t* nimcp_tokenizer_create(const nimcp_tokenizer_config_t* config);
void nimcp_tokenizer_destroy(nimcp_tokenizer_t* tok);

int nimcp_tokenizer_encode(const nimcp_tokenizer_t* tok,
    const char* text, uint32_t* token_ids, uint32_t max_tokens);
int nimcp_tokenizer_decode(const nimcp_tokenizer_t* tok,
    const uint32_t* token_ids, uint32_t num_tokens,
    char* text, uint32_t max_text_length);

const char* nimcp_tokenizer_id_to_text(const nimcp_tokenizer_t* tok, uint32_t id);
uint32_t nimcp_tokenizer_text_to_id(const nimcp_tokenizer_t* tok, const char* text);

int nimcp_tokenizer_train(nimcp_tokenizer_t* tok, const char** texts, uint32_t num_texts);
int nimcp_tokenizer_learn(nimcp_tokenizer_t* tok, const char* text);
int nimcp_tokenizer_learn_from_brain(nimcp_tokenizer_t* tok,
    const char* text, const float* brain_embedding, uint32_t embed_dim);

const float* nimcp_tokenizer_get_embedding(const nimcp_tokenizer_t* tok, uint32_t id);
int nimcp_tokenizer_set_embedding(nimcp_tokenizer_t* tok,
    uint32_t id, const float* embedding, uint32_t embed_dim);

int nimcp_tokenizer_save(const nimcp_tokenizer_t* tok, const char* filepath);
int nimcp_tokenizer_load(nimcp_tokenizer_t* tok, const char* filepath);

uint32_t nimcp_tokenizer_get_vocab_size(const nimcp_tokenizer_t* tok);
uint32_t nimcp_tokenizer_get_num_merges(const nimcp_tokenizer_t* tok);

nimcp_tokenizer_config_t nimcp_tokenizer_config_default(void);

#ifdef __cplusplus
}
#endif
#endif
