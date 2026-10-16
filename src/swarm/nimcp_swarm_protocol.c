/**
 * @file nimcp_swarm_protocol.c
 * @brief NIMCP Swarm Protocol implementation
 *
 * @author NIMCP Development Team
 * @date 2025
 * @version 1.0
 */

#include "swarm/nimcp_swarm_protocol.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "security/nimcp_bbb_helpers.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(swarm_protocol)

#include "utils/thread/nimcp_atomic.h"

//=============================================================================
// Protocol Statistics (Thread-safe with atomic operations)
//=============================================================================

static swarm_protocol_stats_t g_protocol_stats = {0};

/* Thread-safe BBB registration using atomic compare-exchange */
static nimcp_atomic_bool_t g_bbb_registered = {0};

//=============================================================================
// BBB Security Initialization (Thread-safe)
//=============================================================================

/**
 * @brief Initialize BBB security for protocol (thread-safe)
 *
 * Uses atomic compare-exchange to ensure exactly one thread
 * performs the registration, preventing TOCTOU race conditions.
 */
static void swarm_protocol_init_bbb(void)
{
    bool expected = false;
    if (nimcp_atomic_compare_exchange_bool(&g_bbb_registered, &expected, true,
                                           NIMCP_MEMORY_ORDER_ACQ_REL)) {
        bbb_register_module("swarm_protocol", BBB_MODULE_TYPE_SWARM);
        bbb_audit_log(BBB_AUDIT_INFO, "swarm_protocol", "init", "Module registered with BBB");
    }
}

//=============================================================================
// Phoneme Mapping Table
//=============================================================================

/**
 * @brief Phoneme sequence definition for a message type
 */
typedef struct {
    swarm_message_type_t type;
    const char* name;
    const char* word;
    uint8_t phonemes[SWARM_MAX_PHONEMES];
    uint8_t length;
} phoneme_mapping_t;

/**
 * @brief Static phoneme mapping table
 *
 * WHAT: Maps each message type to phoneme sequence
 * WHY:  Provides biologically-inspired message encoding
 * HOW:  Lookup table indexed by message type
 */
static const phoneme_mapping_t g_phoneme_mappings[SWARM_MSG_TYPE_COUNT] = {
    // HEARTBEAT: "hello" /hɛlo/
    {
        SWARM_MSG_HEARTBEAT,
        "HEARTBEAT",
        "HELLO",
        {PHONEME_H, PHONEME_EH, PHONEME_L, PHONEME_OW, 0, 0, 0, 0},
        4
    },

    // THREAT_DETECTED: "danger" /deɪnʤər/
    {
        SWARM_MSG_THREAT_DETECTED,
        "THREAT_DETECTED",
        "DANGER",
        {PHONEME_D, PHONEME_EY, PHONEME_N, PHONEME_JH, PHONEME_ER, 0, 0, 0},
        5
    },

    // TARGET_FOUND: "found" /faʊnd/
    {
        SWARM_MSG_TARGET_FOUND,
        "TARGET_FOUND",
        "FOUND",
        {PHONEME_F, PHONEME_AO, PHONEME_N, PHONEME_D, 0, 0, 0, 0},
        4
    },

    // REQUEST_BACKUP: "help" /hɛlp/
    {
        SWARM_MSG_REQUEST_BACKUP,
        "REQUEST_BACKUP",
        "HELP",
        {PHONEME_H, PHONEME_EH, PHONEME_L, PHONEME_P, 0, 0, 0, 0},
        4
    },

    // FORMATION_CHANGE: "move" /muv/
    {
        SWARM_MSG_FORMATION_CHANGE,
        "FORMATION_CHANGE",
        "MOVE",
        {PHONEME_M, PHONEME_UW, PHONEME_V, 0, 0, 0, 0, 0},
        3
    },

    // REWARD_SIGNAL: "good" /gʊd/
    {
        SWARM_MSG_REWARD_SIGNAL,
        "REWARD_SIGNAL",
        "GOOD",
        {PHONEME_G, PHONEME_UH, PHONEME_D, 0, 0, 0, 0, 0},
        3
    },

    // NEUROMOD_SYNC: "sync" /sɪŋk/
    {
        SWARM_MSG_NEUROMOD_SYNC,
        "NEUROMOD_SYNC",
        "SYNC",
        {PHONEME_S, PHONEME_IH, PHONEME_NG, PHONEME_K, 0, 0, 0, 0},
        4
    },

    // WORKSPACE_BROADCAST: "share" /ʃɛr/
    {
        SWARM_MSG_WORKSPACE_BROADCAST,
        "WORKSPACE_BROADCAST",
        "SHARE",
        {PHONEME_SH, PHONEME_EH, PHONEME_R, 0, 0, 0, 0, 0},
        3
    },

    // VOTE_REQUEST: "vote" /vot/
    {
        SWARM_MSG_VOTE_REQUEST,
        "VOTE_REQUEST",
        "VOTE",
        {PHONEME_V, PHONEME_OW, PHONEME_T, 0, 0, 0, 0, 0},
        3
    },

    // VOTE_RESPONSE: "yes" /jɛs/
    {
        SWARM_MSG_VOTE_RESPONSE,
        "VOTE_RESPONSE",
        "YES",
        {PHONEME_Y, PHONEME_EH, PHONEME_S, 0, 0, 0, 0, 0},
        3
    },

    // SERVER_UPDATE: "set" /sɛt/
    {
        SWARM_MSG_SERVER_UPDATE,
        "SERVER_UPDATE",
        "SET",
        {PHONEME_S, PHONEME_EH, PHONEME_T, 0, 0, 0, 0, 0},
        3
    }
};

//=============================================================================
// CRC16 Implementation
//=============================================================================

/**
 * @brief CRC16-CCITT lookup table (polynomial 0x1021)
 *
 * WHAT: Precomputed CRC values for byte lookups
 * WHY:  Accelerates CRC calculation (table lookup vs bit-by-bit)
 * HOW:  Generated using polynomial 0x1021 with initial value 0
 *
 * GENERATION CODE:
 *   for (int i = 0; i < 256; i++) {
 *       uint16_t crc = i << 8;
 *       for (int j = 0; j < 8; j++) {
 *           crc = (crc << 1) ^ ((crc & 0x8000) ? 0x1021 : 0);
 *       }
 *       table[i] = crc;
 *   }
 */
static const uint16_t crc16_table[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
    0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
    0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
    0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
    0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
    0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
    0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
    0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
    0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
    0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
    0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
    0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
    0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
    0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
    0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
    0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
    0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
    0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
    0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
    0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
    0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
    0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
    0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0
};

uint16_t swarm_protocol_crc16(const uint8_t* data, uint32_t length)
{
    swarm_protocol_init_bbb();

    if (!bbb_check_pointer(data, "swarm_protocol_crc16")) {
        bbb_audit_log(BBB_AUDIT_WARNING, "swarm_protocol", "crc16_error", "Invalid data pointer");
        LOG_ERROR("swarm_protocol_crc16: NULL data pointer");
        return 0;
    }

    if (length > 65535) {
        bbb_audit_log(BBB_AUDIT_WARNING, "swarm_protocol", "crc16_error", "Excessive length: %u", length);
        LOG_ERROR("swarm_protocol_crc16: Excessive length %u", length);
        return 0;
    }

    uint16_t crc = 0xFFFF;  // CRC16-CCITT initial value

    for (uint32_t i = 0; i < length; i++) {
        uint8_t index = (crc >> 8) ^ data[i];
        crc = (crc << 8) ^ crc16_table[index];
    }

    return crc;
}

//=============================================================================
// Internal Helpers
//=============================================================================

/**
 * @brief Calculate CRC for message (excluding CRC field itself)
 *
 * WHAT: Compute CRC over message bytes except the CRC field
 * WHY:  CRC must not include itself in calculation
 * HOW:  Calculate over first (sizeof(msg) - sizeof(crc16)) bytes
 */
static uint16_t calculate_message_crc(const swarm_phoneme_message_t* msg)
{
    // CRC covers everything except the crc16 field itself
    const uint32_t crc_offset = offsetof(swarm_phoneme_message_t, crc16);
    return swarm_protocol_crc16((const uint8_t*)msg, crc_offset);
}

//=============================================================================
// Core Protocol API Implementation
//=============================================================================

nimcp_error_t swarm_protocol_encode(
    swarm_phoneme_message_t* msg,
    swarm_message_type_t type,
    uint16_t sender_id,
    const float* payload,
    uint32_t payload_len)
{
    swarm_protocol_init_bbb();

    // Validate inputs with BBB
    if (!bbb_check_pointer(msg, "swarm_protocol_encode")) {
        bbb_audit_log(BBB_AUDIT_WARNING, "swarm_protocol", "encode_error", "Invalid message pointer");
        LOG_ERROR("swarm_protocol_encode: NULL message pointer");
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (type >= SWARM_MSG_TYPE_COUNT) {
        bbb_audit_log(BBB_AUDIT_WARNING, "swarm_protocol", "encode_error", "Invalid message type %d", type);
        LOG_ERROR("swarm_protocol_encode: Invalid message type %d", type);
        g_protocol_stats.invalid_type_errors++;
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (payload_len > SWARM_PAYLOAD_FLOATS) {
        bbb_audit_log(BBB_AUDIT_WARNING, "swarm_protocol", "encode_error",
                     "Payload length %u exceeds max %d", payload_len, SWARM_PAYLOAD_FLOATS);
        LOG_ERROR("swarm_protocol_encode: Payload length %u exceeds max %d",
                        payload_len, SWARM_PAYLOAD_FLOATS);
        g_protocol_stats.invalid_length_errors++;
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (payload_len > 0 && !bbb_check_pointer(payload, "swarm_protocol_encode")) {
        bbb_audit_log(BBB_AUDIT_WARNING, "swarm_protocol", "encode_error", "NULL payload with non-zero length");
        LOG_ERROR("swarm_protocol_encode: NULL payload with non-zero length");
        return NIMCP_ERROR_NULL_POINTER;
    }

    // Zero-initialize message
    memset(msg, 0, sizeof(swarm_phoneme_message_t));

    // Get phoneme sequence for message type
    const phoneme_mapping_t* mapping = &g_phoneme_mappings[type];

    // Copy phoneme sequence
    memcpy(msg->phoneme_sequence, mapping->phonemes, mapping->length);
    msg->sequence_length = mapping->length;

    // Set message type and sender ID
    msg->message_type = (uint8_t)type;
    msg->sender_id = sender_id;

    // Copy payload
    if (payload_len > 0) {
        memcpy(msg->payload, payload, payload_len * sizeof(float));
    }

    // Calculate and set CRC
    msg->crc16 = calculate_message_crc(msg);

    // Update statistics
    g_protocol_stats.messages_encoded++;

    bbb_audit_log(BBB_AUDIT_DEBUG, "swarm_protocol", "encoded",
                 "Encoded %s message from drone %u (CRC: 0x%04X)",
                 mapping->word, sender_id, msg->crc16);
    LOG_DEBUG("Encoded %s message from drone %u (CRC: 0x%04X)",
                    mapping->word, sender_id, msg->crc16);

    return NIMCP_SUCCESS;
}

nimcp_error_t swarm_protocol_decode(
    const swarm_phoneme_message_t* msg,
    swarm_message_type_t* type,
    uint16_t* sender_id,
    float* payload,
    uint32_t* payload_len)
{
    swarm_protocol_init_bbb();

    // Validate inputs with BBB
    if (!bbb_check_pointer(msg, "swarm_protocol_decode")) {
        bbb_audit_log(BBB_AUDIT_WARNING, "swarm_protocol", "decode_error", "Invalid message pointer");
        LOG_ERROR("swarm_protocol_decode: NULL message pointer");
        return NIMCP_ERROR_NULL_POINTER;
    }

    // Validate network data for potential attack vectors
    if (!bbb_validate_network_data((const uint8_t*)msg, sizeof(*msg), "swarm_protocol_decode")) {
        bbb_audit_log(BBB_AUDIT_CRITICAL, "swarm_protocol", "security_threat",
                     "Malicious network data detected from drone %u", msg->sender_id);
        g_protocol_stats.crc_failures++;
        return NIMCP_ERROR_INVALID_PARAM;
    }

    // Validate CRC first
    if (!swarm_protocol_validate(msg)) {
        bbb_audit_log(BBB_AUDIT_WARNING, "swarm_protocol", "decode_error",
                     "CRC validation failed for message from drone %u", msg->sender_id);
        LOG_ERROR("swarm_protocol_decode: CRC validation failed");
        g_protocol_stats.crc_failures++;
        return NIMCP_ERROR_INVALID_PARAM;
    }

    // Validate message type
    if (msg->message_type >= SWARM_MSG_TYPE_COUNT) {
        LOG_ERROR("swarm_protocol_decode: Invalid message type %u", msg->message_type);
        g_protocol_stats.invalid_type_errors++;
        return NIMCP_ERROR_INVALID_PARAM;
    }

    // Validate sequence length
    if (msg->sequence_length > SWARM_MAX_PHONEMES) {
        LOG_ERROR("swarm_protocol_decode: Invalid sequence length %u", msg->sequence_length);
        g_protocol_stats.invalid_length_errors++;
        return NIMCP_ERROR_INVALID_PARAM;
    }

    // Extract message type
    if (type) {
        *type = (swarm_message_type_t)msg->message_type;
    }

    // Extract sender ID
    if (sender_id) {
        *sender_id = msg->sender_id;
    }

    // Extract payload
    if (payload && payload_len) {
        // Determine actual payload length (how many non-zero floats)
        uint32_t actual_len = 0;
        for (uint32_t i = 0; i < SWARM_PAYLOAD_FLOATS; i++) {
            if (msg->payload[i] != 0.0F || i == 0) {
                // Always copy at least first element, even if zero
                actual_len = i + 1;
            }
        }

        memcpy(payload, msg->payload, actual_len * sizeof(float));
        *payload_len = actual_len;
    }

    // Update statistics
    g_protocol_stats.messages_decoded++;

    bbb_audit_log(BBB_AUDIT_DEBUG, "swarm_protocol", "decoded",
                 "Decoded %s message from drone %u",
                 g_phoneme_mappings[msg->message_type].word, msg->sender_id);
    LOG_DEBUG("Decoded %s message from drone %u",
                    g_phoneme_mappings[msg->message_type].word,
                    msg->sender_id);

    return NIMCP_SUCCESS;
}

bool swarm_protocol_validate(const swarm_phoneme_message_t* msg)
{
    if (!msg) {
        LOG_ERROR("swarm_protocol_validate: NULL message pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_protocol_validate: msg is NULL");
        return false;
    }

    // Calculate expected CRC
    uint16_t calculated_crc = calculate_message_crc(msg);

    // Compare with stored CRC
    bool valid = (calculated_crc == msg->crc16);

    if (!valid) {
        LOG_WARN("CRC mismatch: expected 0x%04X, got 0x%04X",
                       calculated_crc, msg->crc16);
    }

    return valid;
}

nimcp_error_t swarm_protocol_get_phonemes_for_type(
    swarm_message_type_t type,
    uint8_t* phonemes,
    uint32_t* length)
{
    // Validate inputs
    if (!phonemes || !length) {
        LOG_ERROR("swarm_protocol_get_phonemes_for_type: NULL pointer");
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (type >= SWARM_MSG_TYPE_COUNT) {
        LOG_ERROR("swarm_protocol_get_phonemes_for_type: Invalid type %d", type);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    // Get mapping
    const phoneme_mapping_t* mapping = &g_phoneme_mappings[type];

    // Copy phoneme sequence
    memcpy(phonemes, mapping->phonemes, mapping->length);
    *length = mapping->length;

    return NIMCP_SUCCESS;
}

nimcp_error_t swarm_protocol_recognize_type(
    const uint8_t* phonemes,
    uint32_t length,
    swarm_message_type_t* type)
{
    // Validate inputs
    if (!phonemes || !type) {
        LOG_ERROR("swarm_protocol_recognize_type: NULL pointer");
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (length == 0 || length > SWARM_MAX_PHONEMES) {
        LOG_ERROR("swarm_protocol_recognize_type: Invalid length %u", length);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    // Search for matching phoneme sequence
    for (uint32_t i = 0; i < SWARM_MSG_TYPE_COUNT; i++) {
        const phoneme_mapping_t* mapping = &g_phoneme_mappings[i];

        if (mapping->length == length) {
            if (memcmp(phonemes, mapping->phonemes, length) == 0) {
                *type = mapping->type;
                LOG_DEBUG("Recognized phoneme sequence as %s", mapping->word);
                return NIMCP_SUCCESS;
            }
        }
    }

    LOG_WARN("Unknown phoneme sequence (length %u)", length);
    return NIMCP_ERROR_NOT_FOUND;
}

//=============================================================================
// Utility Functions Implementation
//=============================================================================

const char* swarm_protocol_message_type_name(swarm_message_type_t type)
{
    if (type >= SWARM_MSG_TYPE_COUNT) {
        return "UNKNOWN";
    }
    return g_phoneme_mappings[type].name;
}

const char* swarm_protocol_message_type_word(swarm_message_type_t type)
{
    if (type >= SWARM_MSG_TYPE_COUNT) {
        return "UNKNOWN";
    }
    return g_phoneme_mappings[type].word;
}

void swarm_protocol_print_message(const swarm_phoneme_message_t* msg)
{
    if (!msg) {
        printf("NULL message\n");
        return;
    }

    printf("=== Swarm Message ===\n");
    printf("Type:      %s (%s)\n",
           swarm_protocol_message_type_name((swarm_message_type_t)msg->message_type),
           swarm_protocol_message_type_word((swarm_message_type_t)msg->message_type));
    printf("Sender:    %u\n", msg->sender_id);
    printf("Phonemes:  [");
    for (uint32_t i = 0; i < msg->sequence_length; i++) {
        printf("%s%u", i > 0 ? ", " : "", msg->phoneme_sequence[i]);
    }
    printf("] (length=%u)\n", msg->sequence_length);
    printf("Payload:   [%.2f, %.2f, %.2f, %.2f]\n",
           msg->payload[0], msg->payload[1], msg->payload[2], msg->payload[3]);
    printf("CRC16:     0x%04X %s\n",
           msg->crc16,
           swarm_protocol_validate(msg) ? "(valid)" : "(INVALID)");
    printf("====================\n");
}

nimcp_error_t swarm_protocol_get_stats(swarm_protocol_stats_t* stats)
{
    if (!stats) {
        LOG_ERROR("swarm_protocol_get_stats: NULL stats pointer");
        return NIMCP_ERROR_NULL_POINTER;
    }

    memcpy(stats, &g_protocol_stats, sizeof(swarm_protocol_stats_t));
    return NIMCP_SUCCESS;
}

void swarm_protocol_reset_stats(void)
{
    memset(&g_protocol_stats, 0, sizeof(swarm_protocol_stats_t));
    LOG_INFO("Swarm protocol statistics reset");
}

//=============================================================================
// Payload Helper Functions
//=============================================================================

nimcp_error_t swarm_protocol_encode_heartbeat(
    swarm_phoneme_message_t* msg,
    uint16_t sender_id,
    float x,
    float y,
    float z,
    float battery)
{
    float payload[4] = {x, y, z, battery};
    return swarm_protocol_encode(msg, SWARM_MSG_HEARTBEAT, sender_id, payload, 4);
}

nimcp_error_t swarm_protocol_encode_threat(
    swarm_phoneme_message_t* msg,
    uint16_t sender_id,
    float x,
    float y,
    float z,
    float confidence)
{
    float payload[4] = {x, y, z, confidence};
    return swarm_protocol_encode(msg, SWARM_MSG_THREAT_DETECTED, sender_id, payload, 4);
}

nimcp_error_t swarm_protocol_encode_target_found(
    swarm_phoneme_message_t* msg,
    uint16_t sender_id,
    float x,
    float y,
    float z,
    float priority)
{
    float payload[4] = {x, y, z, priority};
    return swarm_protocol_encode(msg, SWARM_MSG_TARGET_FOUND, sender_id, payload, 4);
}

nimcp_error_t swarm_protocol_encode_vote_request(
    swarm_phoneme_message_t* msg,
    uint16_t sender_id,
    float proposal_id,
    float option_count,
    float deadline)
{
    float payload[4] = {proposal_id, option_count, deadline, 0.0F};
    return swarm_protocol_encode(msg, SWARM_MSG_VOTE_REQUEST, sender_id, payload, 4);
}

nimcp_error_t swarm_protocol_encode_vote_response(
    swarm_phoneme_message_t* msg,
    uint16_t sender_id,
    float proposal_id,
    float vote_choice,
    float confidence)
{
    float payload[4] = {proposal_id, vote_choice, confidence, 0.0F};
    return swarm_protocol_encode(msg, SWARM_MSG_VOTE_RESPONSE, sender_id, payload, 4);
}
