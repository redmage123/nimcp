#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_nlp_compression.c - NLP Compression Layer
//=============================================================================
/**
 * @file nimcp_nlp_compression.c
 * @brief Bandwidth-efficient compression for neural protocol messages
 *
 * WHAT: Run-length encoding, dictionary compression, delta encoding
 * WHY:  Minimize bandwidth for constrained/jammed environments
 * HOW:  Multiple compression algorithms selectable by message type
 *
 * COMPRESSION METHODS:
 * - RLE: Run-length encoding for sparse spike trains
 * - Delta: Delta encoding for sequential weight updates
 * - Dict: Dictionary compression for neural language primitives
 * - LZ77: Lightweight LZ77 for general data
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include "networking/nlp/nimcp_neural_link_protocol.h"
#include "networking/nlp/nimcp_neural_language.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "security/nimcp_bbb_helpers.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <stdlib.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(nlp_compression)

#define NLP_COMPRESS_MODULE "nlp_compress"

//=============================================================================
// Compression Types
//=============================================================================

typedef enum {
    NLP_COMPRESS_NONE   = 0x00,  // No compression
    NLP_COMPRESS_RLE    = 0x01,  // Run-length encoding
    NLP_COMPRESS_DELTA  = 0x02,  // Delta encoding
    NLP_COMPRESS_DICT   = 0x03,  // Dictionary compression
    NLP_COMPRESS_LZ77   = 0x04,  // Lightweight LZ77
    NLP_COMPRESS_HYBRID = 0x05,  // Combined approach
} nlp_compress_type_t;

//=============================================================================
// Compression Header
//=============================================================================

typedef struct __attribute__((packed)) {
    uint8_t  type;              // Compression type
    uint8_t  flags;             // Compression flags
    uint16_t original_size;     // Original uncompressed size
    uint16_t compressed_size;   // Compressed size (excluding header)
    uint16_t checksum;          // Adler-32 truncated to 16-bit
} nlp_compress_header_t;

#define NLP_COMPRESS_HEADER_SIZE sizeof(nlp_compress_header_t)

//=============================================================================
// LZ77 Parameters
//=============================================================================

#define LZ77_WINDOW_SIZE     256   // Sliding window (small for embedded)
#define LZ77_MIN_MATCH       3     // Minimum match length
#define LZ77_MAX_MATCH       18    // Maximum match length (4-bit + 3)
#define LZ77_LITERAL_FLAG    0x80  // High bit indicates literal
#define LZ77_MATCH_FLAG      0x00  // High bit clear for match

//=============================================================================
// Dictionary Compression (for Neural Language)
//=============================================================================

/**
 * @brief Common neural expression patterns
 *
 * These represent frequently occurring primitive sequences
 * that can be compressed to single bytes.
 */
static const uint8_t nlang_dict_patterns[][4] = {
    // Pattern 0: COMMAND MOVE THERE
    {INTENT_COMMAND, ACTION_MOVE, SPATIAL_THERE, 0x00},
    // Pattern 1: REPORT THREAT THERE
    {INTENT_REPORT, PERCEPT_THREAT, SPATIAL_THERE, 0x00},
    // Pattern 2: QUERY STATUS
    {INTENT_QUERY, QUERY_STATUS, 0x00, 0x00},
    // Pattern 3: ACKNOWLEDGE SUCCESS
    {INTENT_ACKNOWLEDGE, ASSERT_SUCCESS, 0x00, 0x00},
    // Pattern 4: ACKNOWLEDGE FAILURE
    {INTENT_ACKNOWLEDGE, ASSERT_FAILURE, 0x00, 0x00},
    // Pattern 5: REQUEST HELP IMMEDIATE
    {INTENT_REQUEST, SOCIAL_RESCUER, TEMPORAL_IMMEDIATE, 0x00},
    // Pattern 6: WARN DANGER HERE
    {INTENT_WARN, PERCEPT_THREAT, SPATIAL_HERE, 0x00},
    // Pattern 7: INFORM TARGET THERE
    {INTENT_INFORM, PERCEPT_TARGET, SPATIAL_THERE, 0x00},
    // Pattern 8: COMMAND STOP NOW
    {INTENT_COMMAND, ACTION_STOP, TEMPORAL_NOW, 0x00},
    // Pattern 9: COMMAND RETURN
    {INTENT_COMMAND, ACTION_RETURN, 0x00, 0x00},
    // Pattern 10: REPORT VICTIM
    {INTENT_REPORT, SOCIAL_VICTIM, 0x00, 0x00},
    // Pattern 11: QUERY READY
    {INTENT_QUERY, QUERY_READY, 0x00, 0x00},
    // Pattern 12: ASSERT YES
    {ASSERT_YES, 0x00, 0x00, 0x00},
    // Pattern 13: ASSERT NO
    {ASSERT_NO, 0x00, 0x00, 0x00},
    // Pattern 14: COORDINATE ACTION
    {INTENT_COORDINATE, 0x00, 0x00, 0x00},
    // Pattern 15: ESCALATE PRIORITY
    {INTENT_ESCALATE, 0x00, 0x00, 0x00},
};

#define NLANG_DICT_SIZE (sizeof(nlang_dict_patterns) / sizeof(nlang_dict_patterns[0]))
#define NLANG_DICT_ESCAPE 0xFE  // Escape code for literal

//=============================================================================
// Internal Functions
//=============================================================================

/**
 * @brief Compute Adler-32 checksum (truncated to 16-bit)
 */
static uint16_t compute_adler16(const uint8_t* data, size_t len) {
    uint32_t a = 1, b = 0;

    for (size_t i = 0; i < len; i++) {
        a = (a + data[i]) % 65521;
        b = (b + a) % 65521;
    }

    return (uint16_t)((b << 8) | (a & 0xFF));
}

/**
 * @brief Find longest match in LZ77 window
 */
static int lz77_find_match(const uint8_t* data, size_t pos, size_t len,
                           uint8_t* match_offset, uint8_t* match_len) {
    if (pos < LZ77_MIN_MATCH || len - pos < LZ77_MIN_MATCH) {
        return 0;
    }

    size_t window_start = (pos > LZ77_WINDOW_SIZE) ? pos - LZ77_WINDOW_SIZE : 0;
    int best_len = 0;
    size_t best_offset = 0;

    for (size_t i = window_start; i < pos; i++) {
        int match = 0;
        while (match < LZ77_MAX_MATCH &&
               pos + match < len &&
               data[i + match] == data[pos + match]) {
            match++;
        }

        if (match >= LZ77_MIN_MATCH && match > best_len) {
            best_len = match;
            best_offset = pos - i;
        }
    }

    if (best_len >= LZ77_MIN_MATCH) {
        *match_offset = (uint8_t)best_offset;
        *match_len = (uint8_t)(best_len - LZ77_MIN_MATCH);  // Store as offset from min
        return best_len;
    }

    return 0;
}

//=============================================================================
// Run-Length Encoding (for sparse spike trains)
//=============================================================================

/**
 * @brief RLE compress data
 *
 * Format: [count] [value] pairs
 * If count == 0, next byte is literal (escape)
 */
static int rle_compress(const uint8_t* input, size_t input_len,
                        uint8_t* output, size_t output_max) {
    if (!input || !output || input_len == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nlp_compression: required parameter is NULL (input, output)");
        return -1;
    }

    size_t in_pos = 0;
    size_t out_pos = 0;

    while (in_pos < input_len && out_pos < output_max - 1) {
        uint8_t current = input[in_pos];
        size_t run_len = 1;

        // Count consecutive identical bytes
        while (in_pos + run_len < input_len &&
               input[in_pos + run_len] == current &&
               run_len < 255) {
            run_len++;
        }

        if (run_len >= 3 || current == 0) {
            // Encode as run
            if (out_pos + 2 > output_max) break;
            output[out_pos++] = (uint8_t)run_len;
            output[out_pos++] = current;
        } else {
            // Encode as literals (count=0 escape)
            for (size_t i = 0; i < run_len && out_pos + 2 <= output_max; i++) {
                output[out_pos++] = 0;  // Escape
                output[out_pos++] = current;
            }
        }

        in_pos += run_len;
    }

    return (in_pos == input_len) ? (int)out_pos : -1;
}

/**
 * @brief RLE decompress data
 */
static int rle_decompress(const uint8_t* input, size_t input_len,
                          uint8_t* output, size_t output_max) {
    if (!input || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nlp_compression: required parameter is NULL (input, output)");
        return -1;
    }

    size_t in_pos = 0;
    size_t out_pos = 0;

    while (in_pos + 1 < input_len && out_pos < output_max) {
        uint8_t count = input[in_pos++];
        uint8_t value = input[in_pos++];

        if (count == 0) {
            // Literal (escaped)
            if (out_pos >= output_max) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "rle_decompress: output buffer overflow");
                return -1;
            }
            output[out_pos++] = value;
        } else {
            // Run
            for (uint8_t i = 0; i < count && out_pos < output_max; i++) {
                output[out_pos++] = value;
            }
        }
    }

    return (int)out_pos;
}

//=============================================================================
// Delta Encoding (for weight updates)
//=============================================================================

/**
 * @brief Delta compress sequential values
 *
 * Stores first value, then deltas from previous
 */
static int delta_compress(const uint8_t* input, size_t input_len,
                          uint8_t* output, size_t output_max) {
    if (!input || !output || input_len == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nlp_compression: required parameter is NULL (input, output)");
        return -1;
    }
    if (output_max < input_len) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "delta_compress: output buffer too small");
        return -1;
    }

    output[0] = input[0];  // First value as-is

    for (size_t i = 1; i < input_len; i++) {
        output[i] = input[i] - input[i - 1];  // Delta
    }

    return (int)input_len;
}

/**
 * @brief Delta decompress sequential values
 */
static int delta_decompress(const uint8_t* input, size_t input_len,
                            uint8_t* output, size_t output_max) {
    if (!input || !output || input_len == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nlp_compression: required parameter is NULL (input, output)");
        return -1;
    }
    if (output_max < input_len) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "delta_compress: output buffer too small");
        return -1;
    }

    output[0] = input[0];  // First value as-is

    for (size_t i = 1; i < input_len; i++) {
        output[i] = output[i - 1] + input[i];  // Accumulate deltas
    }

    return (int)input_len;
}

//=============================================================================
// Dictionary Compression (for Neural Language)
//=============================================================================

/**
 * @brief Get pattern length (excluding null terminator)
 */
static int get_pattern_len(const uint8_t* pattern) {
    int len = 0;
    while (len < 4 && pattern[len] != 0x00) {
        len++;
    }
    return len;
}

/**
 * @brief Find dictionary pattern match
 */
static int dict_find_pattern(const uint8_t* data, size_t len) {
    for (size_t p = 0; p < NLANG_DICT_SIZE; p++) {
        int plen = get_pattern_len(nlang_dict_patterns[p]);
        if (plen == 0) continue;

        if ((size_t)plen <= len) {
            bool match = true;
            for (int i = 0; i < plen && match; i++) {
                if (data[i] != nlang_dict_patterns[p][i]) {
                    match = false;
                }
            }
            if (match) {
                return (int)p;
            }
        }
    }
    /* Pattern not found in dictionary is normal lookup miss, not an error */
    return -1;
}

/**
 * @brief Dictionary compress neural language primitives
 */
static int dict_compress(const uint8_t* input, size_t input_len,
                         uint8_t* output, size_t output_max) {
    if (!input || !output || input_len == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nlp_compression: required parameter is NULL (input, output)");
        return -1;
    }

    size_t in_pos = 0;
    size_t out_pos = 0;

    while (in_pos < input_len && out_pos < output_max) {
        int pattern_idx = dict_find_pattern(&input[in_pos], input_len - in_pos);

        if (pattern_idx >= 0) {
            // Found pattern - emit pattern index
            output[out_pos++] = (uint8_t)pattern_idx;
            in_pos += get_pattern_len(nlang_dict_patterns[pattern_idx]);
        } else {
            // No pattern - emit escape + literal
            if (out_pos + 2 > output_max) break;
            output[out_pos++] = NLANG_DICT_ESCAPE;
            output[out_pos++] = input[in_pos++];
        }
    }

    return (in_pos == input_len) ? (int)out_pos : -1;
}

/**
 * @brief Dictionary decompress neural language primitives
 */
static int dict_decompress(const uint8_t* input, size_t input_len,
                           uint8_t* output, size_t output_max) {
    if (!input || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nlp_compression: required parameter is NULL (input, output)");
        return -1;
    }

    size_t in_pos = 0;
    size_t out_pos = 0;

    while (in_pos < input_len && out_pos < output_max) {
        uint8_t code = input[in_pos++];

        if (code == NLANG_DICT_ESCAPE) {
            // Literal follows
            if (in_pos >= input_len) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nlp_decompress: truncated data");
                return -1;
            }
            output[out_pos++] = input[in_pos++];
        } else if (code < NLANG_DICT_SIZE) {
            // Pattern index
            int plen = get_pattern_len(nlang_dict_patterns[code]);
            if (out_pos + plen > output_max) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nlp_decompress: validation failed");
                return -1;
            }

            for (int i = 0; i < plen; i++) {
                output[out_pos++] = nlang_dict_patterns[code][i];
            }
        } else {
            // Unknown code - treat as literal
            output[out_pos++] = code;
        }
    }

    return (int)out_pos;
}

//=============================================================================
// LZ77 Compression (general purpose)
//=============================================================================

/**
 * @brief LZ77 compress data
 */
static int lz77_compress(const uint8_t* input, size_t input_len,
                         uint8_t* output, size_t output_max) {
    if (!input || !output || input_len == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nlp_compression: required parameter is NULL (input, output)");
        return -1;
    }

    size_t in_pos = 0;
    size_t out_pos = 0;

    while (in_pos < input_len && out_pos < output_max - 2) {
        uint8_t match_offset, match_len;
        int match = lz77_find_match(input, in_pos, input_len,
                                    &match_offset, &match_len);

        if (match > 0) {
            // Emit match: [flag|len_offset] [offset]
            // flag = 0, len_offset = match_len - MIN_MATCH (4 bits max)
            output[out_pos++] = LZ77_MATCH_FLAG | (match_len & 0x0F);
            output[out_pos++] = match_offset;
            in_pos += match;
        } else {
            // Emit literal: [flag|literal]
            output[out_pos++] = LZ77_LITERAL_FLAG | (input[in_pos++] & 0x7F);
        }
    }

    // Handle remaining bytes as literals
    while (in_pos < input_len && out_pos < output_max) {
        output[out_pos++] = LZ77_LITERAL_FLAG | (input[in_pos++] & 0x7F);
    }

    return (in_pos == input_len) ? (int)out_pos : -1;
}

/**
 * @brief LZ77 decompress data
 */
static int lz77_decompress(const uint8_t* input, size_t input_len,
                           uint8_t* output, size_t output_max,
                           size_t expected_size) {
    if (!input || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nlp_compression: required parameter is NULL (input, output)");
        return -1;
    }

    size_t in_pos = 0;
    size_t out_pos = 0;

    while (in_pos < input_len && out_pos < output_max) {
        uint8_t code = input[in_pos++];

        if (code & LZ77_LITERAL_FLAG) {
            // Literal (7-bit value in low bits)
            output[out_pos++] = code & 0x7F;
        } else {
            // Match: next byte is offset
            if (in_pos >= input_len) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nlp_decompress: truncated data");
                return -1;
            }
            uint8_t offset = input[in_pos++];
            uint8_t length = (code & 0x0F) + LZ77_MIN_MATCH;

            if (offset > out_pos) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nlp_decompress: validation failed");
                return -1;
            }

            size_t src_pos = out_pos - offset;
            for (int i = 0; i < length && out_pos < output_max; i++) {
                output[out_pos++] = output[src_pos + i];
            }
        }
    }

    (void)expected_size;
    return (int)out_pos;
}

//=============================================================================
// Public API
//=============================================================================

/**
 * @brief Select best compression method for data type
 */
static nlp_compress_type_t select_compression(nlp_msg_type_t msg_type,
                                              const uint8_t* data,
                                              size_t len) {
    (void)data;  // Could analyze data for better selection

    // Select based on message type
    switch (msg_type) {
        case NLP_MSG_SPIKE_BATCH:
            return NLP_COMPRESS_RLE;  // Sparse spike trains

        case NLP_MSG_WEIGHT_DELTA:
        case NLP_MSG_GRADIENT_PUSH:
            return NLP_COMPRESS_DELTA;  // Sequential weight changes

        case NLP_MSG_STATE_SYNC:
        case NLP_MSG_PEER_ANNOUNCE:
            // For neural language, use dictionary
            if (len < 64) {
                return NLP_COMPRESS_DICT;
            }
            return NLP_COMPRESS_LZ77;

        default:
            // General data
            if (len < 32) {
                return NLP_COMPRESS_NONE;  // Too small
            }
            return NLP_COMPRESS_LZ77;
    }
}

/**
 * @brief Compress NLP message payload
 *
 * @param msg_type Message type (for compression selection)
 * @param input Input data
 * @param input_len Input length
 * @param output Output buffer (must be input_len + header)
 * @param output_max Output buffer size
 * @return Compressed size including header, or -1 on error
 */
int nlp_compress(nlp_msg_type_t msg_type,
                 const uint8_t* input, size_t input_len,
                 uint8_t* output, size_t output_max) {
    if (!input || !output || input_len == 0) {
        NIMCP_LOGGING_ERROR("nlp_compress", "Invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nlp_compression: required parameter is NULL (input, output)");
        return -1;
    }

    if (output_max < input_len + NLP_COMPRESS_HEADER_SIZE) {
        NIMCP_LOGGING_ERROR("nlp_compress", "Output buffer too small");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nlp_compression: validation failed");
        return -1;
    }

    // BBB validation - check input data bounds
    if (input_len > NLP_MAX_PAYLOAD) {
        bbb_audit_log(BBB_AUDIT_WARNING, NLP_COMPRESS_MODULE, "oversized_input",
                      "Input size %zu exceeds max %d", input_len, NLP_MAX_PAYLOAD);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nlp_compression: validation failed");
        return -1;
    }

    // Select compression method
    nlp_compress_type_t type = select_compression(msg_type, input, input_len);

    // Prepare output space after header
    uint8_t* compressed_data = output + NLP_COMPRESS_HEADER_SIZE;
    size_t compressed_max = output_max - NLP_COMPRESS_HEADER_SIZE;
    int compressed_len = -1;

    // Try compression
    switch (type) {
        case NLP_COMPRESS_RLE:
            compressed_len = rle_compress(input, input_len,
                                          compressed_data, compressed_max);
            break;

        case NLP_COMPRESS_DELTA:
            compressed_len = delta_compress(input, input_len,
                                            compressed_data, compressed_max);
            break;

        case NLP_COMPRESS_DICT:
            compressed_len = dict_compress(input, input_len,
                                           compressed_data, compressed_max);
            break;

        case NLP_COMPRESS_LZ77:
            compressed_len = lz77_compress(input, input_len,
                                           compressed_data, compressed_max);
            break;

        default:
            type = NLP_COMPRESS_NONE;
            break;
    }

    // If compression didn't help, store uncompressed
    if (compressed_len < 0 || (size_t)compressed_len >= input_len) {
        type = NLP_COMPRESS_NONE;
        memcpy(compressed_data, input, input_len);
        compressed_len = (int)input_len;
    }

    // Build header
    nlp_compress_header_t* header = (nlp_compress_header_t*)output;
    header->type = (uint8_t)type;
    header->flags = 0;
    header->original_size = (uint16_t)input_len;
    header->compressed_size = (uint16_t)compressed_len;
    header->checksum = compute_adler16(input, input_len);

    size_t total_size = NLP_COMPRESS_HEADER_SIZE + compressed_len;

    NIMCP_LOGGING_DEBUG("nlp_compress", "Compressed %zu -> %zu bytes (%.1f%%) using %s",
                    input_len, total_size,
                    100.0F * total_size / input_len,
                    type == NLP_COMPRESS_NONE ? "none" :
                    type == NLP_COMPRESS_RLE ? "RLE" :
                    type == NLP_COMPRESS_DELTA ? "delta" :
                    type == NLP_COMPRESS_DICT ? "dict" : "LZ77");

    return (int)total_size;
}

/**
 * @brief Decompress NLP message payload
 *
 * @param input Compressed data (including header)
 * @param input_len Compressed data length
 * @param output Output buffer
 * @param output_max Output buffer size
 * @return Decompressed size, or -1 on error
 */
int nlp_decompress(const uint8_t* input, size_t input_len,
                   uint8_t* output, size_t output_max) {
    if (!input || !output) {
        NIMCP_LOGGING_ERROR("nlp_decompress", "Invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nlp_compression: required parameter is NULL (input, output)");
        return -1;
    }

    if (input_len < NLP_COMPRESS_HEADER_SIZE) {
        NIMCP_LOGGING_ERROR("nlp_decompress", "Input too small for header");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nlp_compression: validation failed");
        return -1;
    }

    // Parse header
    const nlp_compress_header_t* header = (const nlp_compress_header_t*)input;
    const uint8_t* compressed_data = input + NLP_COMPRESS_HEADER_SIZE;
    size_t compressed_len = header->compressed_size;

    // BBB validation - check for malicious decompression bombs
    if (header->original_size > NLP_MAX_PAYLOAD) {
        bbb_audit_log(BBB_AUDIT_WARNING, NLP_COMPRESS_MODULE, "decompression_bomb",
                      "Claimed original size %u exceeds max %d - possible attack",
                      header->original_size, NLP_MAX_PAYLOAD);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nlp_compression: validation failed");
        return -1;
    }

    if (NLP_COMPRESS_HEADER_SIZE + compressed_len > input_len) {
        NIMCP_LOGGING_ERROR("nlp_decompress", "Truncated compressed data");
        bbb_audit_log(BBB_AUDIT_WARNING, NLP_COMPRESS_MODULE, "truncated_data",
                      "Compressed data truncated - header claims %zu, have %zu",
                      compressed_len, input_len - NLP_COMPRESS_HEADER_SIZE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nlp_compression: validation failed");
        return -1;
    }

    if (header->original_size > output_max) {
        NIMCP_LOGGING_ERROR("nlp_decompress", "Output buffer too small");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nlp_compression: validation failed");
        return -1;
    }

    int decompressed_len = -1;

    // Decompress based on type
    switch (header->type) {
        case NLP_COMPRESS_NONE:
            memcpy(output, compressed_data, compressed_len);
            decompressed_len = (int)compressed_len;
            break;

        case NLP_COMPRESS_RLE:
            decompressed_len = rle_decompress(compressed_data, compressed_len,
                                              output, output_max);
            break;

        case NLP_COMPRESS_DELTA:
            decompressed_len = delta_decompress(compressed_data, compressed_len,
                                                output, output_max);
            break;

        case NLP_COMPRESS_DICT:
            decompressed_len = dict_decompress(compressed_data, compressed_len,
                                               output, output_max);
            break;

        case NLP_COMPRESS_LZ77:
            decompressed_len = lz77_decompress(compressed_data, compressed_len,
                                               output, output_max,
                                               header->original_size);
            break;

        default:
            NIMCP_LOGGING_ERROR("nlp_decompress", "Unknown compression type %u",
                           header->type);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nlp_decompress: unknown compression type");
            return -1;
    }

    if (decompressed_len < 0) {
        NIMCP_LOGGING_ERROR("nlp_decompress", "Decompression failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nlp_compression: validation failed");
        return -1;
    }

    // Verify checksum
    uint16_t checksum = compute_adler16(output, (size_t)decompressed_len);
    if (checksum != header->checksum) {
        NIMCP_LOGGING_ERROR("nlp_decompress", "Checksum mismatch: 0x%04X != 0x%04X",
                        checksum, header->checksum);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nlp_compression: validation failed");
        return -1;
    }

    NIMCP_LOGGING_DEBUG("nlp_decompress", "Decompressed %zu -> %d bytes",
                    compressed_len, decompressed_len);

    return decompressed_len;
}

/**
 * @brief Get compression type name
 */
const char* nlp_compress_type_name(uint8_t type) {
    switch (type) {
        case NLP_COMPRESS_NONE:   return "none";
        case NLP_COMPRESS_RLE:    return "RLE";
        case NLP_COMPRESS_DELTA:  return "delta";
        case NLP_COMPRESS_DICT:   return "dictionary";
        case NLP_COMPRESS_LZ77:   return "LZ77";
        case NLP_COMPRESS_HYBRID: return "hybrid";
        default: return "unknown";
    }
}

/**
 * @brief Get compressed payload info without decompressing
 */
int nlp_compress_get_info(const uint8_t* data, size_t len,
                          size_t* original_size, size_t* compressed_size,
                          uint8_t* compress_type) {
    if (!data || len < NLP_COMPRESS_HEADER_SIZE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "nlp_compress_get_info: invalid parameters");

            return -1;
    }

    const nlp_compress_header_t* header = (const nlp_compress_header_t*)data;

    if (original_size) *original_size = header->original_size;
    if (compressed_size) *compressed_size = header->compressed_size;
    if (compress_type) *compress_type = header->type;

    return 0;
}
