/**
 * @file nimcp_tokenizer.h
 * @brief Word-level tokenizer with BPE subword fallback for language generation
 *
 * WHAT: Tokenizer that maps text to integer token IDs and back
 * WHY:  Foundation for all language generation — text must be tokenized before
 *        embedding lookup or neural processing
 * HOW:  Hash table for O(1) token-to-id lookups, BPE merge rules for subword
 *        segmentation of out-of-vocabulary words
 *
 * ARCHITECTURE:
 *   Text Input
 *       |
 *       v
 *   [Normalize + Split] --> word tokens
 *       |
 *       v
 *   [Vocab Lookup] -----> known word? --> token ID
 *       |                     no
 *       v
 *   [BPE Merge Rules] ----> subword tokens --> token IDs
 *       |                     unknown
 *       v
 *   [UNK fallback] -------> UNK token ID
 *
 * @author NIMCP Development Team
 * @date 2026-02-25
 * @version 1.0.0
 */

#ifndef NIMCP_TOKENIZER_H
#define NIMCP_TOKENIZER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Constants
 *===========================================================================*/

/** Default initial vocabulary capacity */
#define TOKENIZER_DEFAULT_VOCAB_CAPACITY  32768

/** Default maximum token string length */
#define TOKENIZER_DEFAULT_MAX_TOKEN_LEN   64

/** Magic number for serialization validation */
#define TOKENIZER_MAGIC  0x544F4B4E  /* "TOKN" */

/** Serialization format version */
#define TOKENIZER_VERSION  1

/** Invalid token ID sentinel */
#define TOKENIZER_INVALID_ID  UINT32_MAX

/*=============================================================================
 * Types
 *===========================================================================*/

/** Opaque tokenizer handle */
typedef struct tokenizer tokenizer_t;

/**
 * WHAT: Tokenizer configuration
 * WHY:  Allow callers to customize vocabulary size, special tokens, etc.
 * HOW:  Pass to tokenizer_create(); NULL fields use defaults
 */
typedef struct {
    uint32_t    initial_vocab_capacity;  /**< Initial hash table + array capacity (default 32768) */
    uint32_t    max_token_length;        /**< Maximum bytes per token string (default 64) */
    const char* unk_token;               /**< Unknown token string (default "<UNK>") */
    const char* bos_token;               /**< Beginning-of-sequence token (default "<BOS>") */
    const char* eos_token;               /**< End-of-sequence token (default "<EOS>") */
    const char* pad_token;               /**< Padding token (default "<PAD>") */
} tokenizer_config_t;

/*=============================================================================
 * Configuration
 *===========================================================================*/

/**
 * WHAT: Return a tokenizer_config_t with all default values
 * WHY:  Callers can start from defaults and override only what they need
 * HOW:  Returns stack-allocated struct with sensible defaults
 *
 * @return Default configuration
 */
tokenizer_config_t tokenizer_default_config(void);

/*=============================================================================
 * Lifecycle
 *===========================================================================*/

/**
 * WHAT: Create a new tokenizer from configuration
 * WHY:  Allocates vocabulary arrays, hash table, special tokens
 * HOW:  Allocates id_to_token array, creates hash_table for reverse lookup,
 *        registers the four special tokens at IDs 0-3
 *
 * @param config Configuration (NULL for defaults)
 * @return Tokenizer handle or NULL on failure
 */
tokenizer_t* tokenizer_create(const tokenizer_config_t* config);

/**
 * WHAT: Destroy tokenizer and free all resources
 * WHY:  Clean up vocabulary strings, hash table, merge rules
 * HOW:  Frees each token string, destroys hash table, frees arrays
 *
 * @param tok Tokenizer to destroy (NULL is safe)
 */
void tokenizer_destroy(tokenizer_t* tok);

/*=============================================================================
 * Vocabulary Management
 *===========================================================================*/

/**
 * WHAT: Add a single token to the vocabulary
 * WHY:  Manual vocabulary construction or extension
 * HOW:  Checks for duplicates via hash table, grows arrays if needed,
 *        stores nimcp_strdup'd copy, inserts into hash table
 *
 * @param tok  Tokenizer
 * @param token  Token string to add
 * @return Assigned token ID on success, -1 on error (NULL args, alloc failure, duplicate)
 */
int tokenizer_add_token(tokenizer_t* tok, const char* token);

/**
 * WHAT: Add a BPE merge rule
 * WHY:  Merge rules are applied in order during encoding to segment OOV words
 * HOW:  Stores "token_a token_b" as a single string in merge_rules array
 *
 * @param tok      Tokenizer
 * @param token_a  First token of the merge pair
 * @param token_b  Second token of the merge pair
 * @return 0 on success, -1 on error
 */
int tokenizer_add_merge_rule(tokenizer_t* tok, const char* token_a, const char* token_b);

/**
 * WHAT: Build vocabulary from corpus text using BPE-like algorithm
 * WHY:  Automatically learns subword vocabulary from data
 * HOW:  1. Initialize with character-level vocab from text
 *        2. Count adjacent pair frequencies across all words
 *        3. Merge most frequent pair, add to vocab
 *        4. Repeat until target_vocab_size reached
 *        5. Store merge rules for use during encoding
 *
 * @param tok               Tokenizer (should be freshly created)
 * @param text              Training corpus text
 * @param target_vocab_size Desired vocabulary size
 * @return 0 on success, -1 on error
 */
int tokenizer_build_from_text(tokenizer_t* tok, const char* text, uint32_t target_vocab_size);

/*=============================================================================
 * Encoding / Decoding
 *===========================================================================*/

/**
 * WHAT: Encode text into token IDs
 * WHY:  Convert human-readable text into integer sequences for neural processing
 * HOW:  1. Lowercase and split on whitespace/punctuation
 *        2. Look up each word in vocabulary hash table
 *        3. If not found, split into characters and apply BPE merge rules
 *        4. Map remaining unknowns to UNK
 *
 * @param tok         Tokenizer
 * @param text        Input text to encode
 * @param token_ids   Output array for token IDs (caller-allocated)
 * @param max_tokens  Capacity of token_ids array
 * @param num_tokens  [OUT] Number of tokens written
 * @return 0 on success, -1 on error
 */
int tokenizer_encode(const tokenizer_t* tok, const char* text,
                     uint32_t* token_ids, uint32_t max_tokens, uint32_t* num_tokens);

/**
 * WHAT: Decode token IDs back into text
 * WHY:  Convert neural output back to human-readable string
 * HOW:  Look up each token ID, join with spaces, handle subword markers
 *
 * @param tok         Tokenizer
 * @param token_ids   Array of token IDs
 * @param num_tokens  Number of token IDs
 * @param text        Output text buffer (caller-allocated)
 * @param max_length  Capacity of text buffer
 * @return 0 on success, -1 on error
 */
int tokenizer_decode(const tokenizer_t* tok, const uint32_t* token_ids,
                     uint32_t num_tokens, char* text, uint32_t max_length);

/*=============================================================================
 * Lookups
 *===========================================================================*/

/**
 * WHAT: Get current vocabulary size
 * WHY:  Needed for embedding layer dimensioning
 *
 * @param tok Tokenizer
 * @return Vocabulary size, or 0 if tok is NULL
 */
uint32_t tokenizer_get_vocab_size(const tokenizer_t* tok);

/**
 * WHAT: Look up a token string and return its ID
 * WHY:  O(1) lookup via hash table
 *
 * @param tok    Tokenizer
 * @param token  Token string
 * @return Token ID, or TOKENIZER_INVALID_ID if not found
 */
uint32_t tokenizer_token_to_id(const tokenizer_t* tok, const char* token);

/**
 * WHAT: Look up a token ID and return its string
 * WHY:  O(1) lookup via array index
 *
 * @param tok  Tokenizer
 * @param id   Token ID
 * @return Token string (internal pointer, do not free), or NULL if invalid
 */
const char* tokenizer_id_to_token(const tokenizer_t* tok, uint32_t id);

/*=============================================================================
 * Serialization
 *===========================================================================*/

/**
 * WHAT: Save tokenizer to binary file
 * WHY:  Persist trained vocabulary and merge rules for reuse
 * HOW:  Writes magic, version, vocab, merge rules in binary format
 *
 * @param tok   Tokenizer
 * @param path  File path to write
 * @return 0 on success, -1 on error
 */
int tokenizer_save(const tokenizer_t* tok, const char* path);

/**
 * WHAT: Load tokenizer from binary file
 * WHY:  Restore previously trained tokenizer
 * HOW:  Reads and validates magic/version, reconstructs vocab and merge rules
 *
 * @param path  File path to read
 * @return Tokenizer handle or NULL on error
 */
tokenizer_t* tokenizer_load(const char* path);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TOKENIZER_H */
