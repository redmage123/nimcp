/**
 * @file nimcp_swarm_protocol.h
 * @brief NIMCP Swarm Protocol for phoneme-based drone-to-drone communication
 *
 * WHAT: Protocol for drone swarm communication using speech cortex phonemes
 * WHY:  Enable biologically-inspired swarm coordination with compact messaging
 * HOW:  Map message types to phoneme sequences for efficient encoding/transmission
 *
 * BIOLOGICAL INSPIRATION:
 * - Phonemes are atomic speech units (~44 in English, highly distinctive)
 * - Speech cortex can recognize phonemes in noisy environments (cocktail party effect)
 * - Phoneme sequences provide error-resistant communication channel
 * - Analogous to bee waggle dance or bird song for swarm coordination
 *
 * DESIGN PRINCIPLES:
 * - Compact: 24-byte messages with 8-phoneme payload
 * - Error-resistant: CRC16 checksum + phoneme redundancy
 * - Biologically-plausible: Maps to existing speech cortex infrastructure
 * - Extensible: 11 message types with room for expansion
 *
 * MESSAGE FORMAT (24 bytes):
 * +-----------------+------------------+---------------+----------------+
 * | Phoneme Seq (8) | Seq Len (1)      | Msg Type (1)  | Sender ID (2)  |
 * +-----------------+------------------+---------------+----------------+
 * | Payload Float[4] (16 bytes)                       | CRC16 (2)      |
 * +---------------------------------------------------+----------------+
 *
 * PHONEME ENCODING EXAMPLES:
 * - Heartbeat:  /hɛlo/   (HELLO)     → "I am alive"
 * - Threat:     /deɪnʤər/ (DANGER)   → "Enemy detected"
 * - Target:     /faʊnd/   (FOUND)    → "Target acquired"
 * - Backup:     /hɛlp/    (HELP)     → "Need assistance"
 *
 * USAGE:
 *   swarm_phoneme_message_t msg;
 *   swarm_protocol_encode(&msg, SWARM_MSG_HEARTBEAT, 42, position, 4);
 *   // Transmit msg over radio/acoustic channel
 *   // On receive:
 *   if (swarm_protocol_validate(&msg)) {
 *       swarm_protocol_decode(&msg, &type, &sender, payload, &len);
 *   }
 *
 * @author NIMCP Development Team
 * @date 2025
 * @version 1.0
 */

#ifndef NIMCP_SWARM_PROTOCOL_H
#define NIMCP_SWARM_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include "perception/nimcp_speech_cortex.h"
#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Configuration Constants
//=============================================================================

/** Maximum phonemes in a swarm message */
#define SWARM_MAX_PHONEMES 8

/** Swarm message total size (bytes) */
#define SWARM_MESSAGE_SIZE 24

/** Payload floats per message */
#define SWARM_PAYLOAD_FLOATS 4

/** Maximum swarm size (drone count) */
#define SWARM_MAX_DRONES 65535

//=============================================================================
// Message Type Enumeration
//=============================================================================

/**
 * @brief Swarm message types
 *
 * WHAT: Enumeration of all swarm coordination message types
 * WHY:  Define semantic categories for drone-to-drone communication
 * HOW:  Each type maps to a distinct phoneme sequence
 */
#ifndef SWARM_MESSAGE_TYPE_DEFINED
#define SWARM_MESSAGE_TYPE_DEFINED
typedef enum {
    SWARM_MSG_HEARTBEAT = 0,          ///< Periodic "I'm alive" signal
    SWARM_MSG_THREAT_DETECTED = 1,    ///< Enemy/obstacle detected
    SWARM_MSG_TARGET_FOUND = 2,       ///< Target/objective located
    SWARM_MSG_REQUEST_BACKUP = 3,     ///< Request assistance from swarm
    SWARM_MSG_FORMATION_CHANGE = 4,   ///< Change formation (e.g., V → line)
    SWARM_MSG_REWARD_SIGNAL = 5,      ///< Positive reward (good state)
    SWARM_MSG_NEUROMOD_SYNC = 6,      ///< Synchronize neuromodulator levels
    SWARM_MSG_WORKSPACE_BROADCAST = 7,///< Broadcast from global workspace
    SWARM_MSG_VOTE_REQUEST = 8,       ///< Request vote for consensus
    SWARM_MSG_VOTE_RESPONSE = 9,      ///< Vote response (yes/no/abstain)
    SWARM_MSG_SERVER_UPDATE = 10,     ///< Server-to-swarm update

    SWARM_MSG_TYPE_COUNT              ///< Total message types
} swarm_message_type_t;
#endif

//=============================================================================
// Message Structure
//=============================================================================

/**
 * @brief Phoneme-based swarm message (24 bytes total)
 *
 * WHAT: Compact message format using speech cortex phonemes
 * WHY:  Enable efficient, error-resistant drone communication
 * HOW:  8 phonemes + metadata + 4 floats + CRC16
 *
 * LAYOUT (24 bytes):
 * - Bytes 0-7:   Phoneme sequence (uint8_t[8])
 * - Byte  8:     Sequence length (1-8)
 * - Byte  9:     Message type (swarm_message_type_t)
 * - Bytes 10-11: Sender ID (uint16_t)
 * - Bytes 12-27: Payload (4 × float = 16 bytes)
 * - Bytes 28-29: CRC16 checksum
 *
 * ALIGNMENT: Naturally aligned (no padding needed)
 */
typedef struct {
    uint8_t phoneme_sequence[SWARM_MAX_PHONEMES];  ///< Phoneme IDs (phoneme_t)
    uint8_t sequence_length;                       ///< Valid phonemes (1-8)
    uint8_t message_type;                          ///< swarm_message_type_t
    uint16_t sender_id;                            ///< Drone ID (0-65535)
    float payload[SWARM_PAYLOAD_FLOATS];           ///< Type-specific data
    uint16_t crc16;                                ///< CRC16-CCITT checksum
} swarm_phoneme_message_t;

/**
 * @brief Swarm protocol statistics
 */
typedef struct {
    uint64_t messages_encoded;        ///< Total messages encoded
    uint64_t messages_decoded;        ///< Total messages decoded
    uint64_t crc_failures;            ///< CRC validation failures
    uint64_t invalid_type_errors;     ///< Invalid message type errors
    uint64_t invalid_length_errors;   ///< Invalid sequence length errors
} swarm_protocol_stats_t;

//=============================================================================
// Core Protocol API
//=============================================================================

/**
 * @brief Encode swarm message
 *
 * WHAT: Create phoneme-based swarm message
 * WHY:  Prepare message for transmission to other drones
 * HOW:  Map message type → phonemes, pack payload, calculate CRC
 *
 * ALGORITHM:
 * 1. Validate inputs (type, payload length)
 * 2. Get phoneme sequence for message type
 * 3. Copy phonemes to message
 * 4. Pack sender ID and payload
 * 5. Calculate and set CRC16
 *
 * @param msg Output message structure (must be pre-allocated)
 * @param type Message type (SWARM_MSG_*)
 * @param sender_id Sending drone ID
 * @param payload Payload data (float array, can be NULL if payload_len=0)
 * @param payload_len Number of payload floats (0-4)
 * @return NIMCP_SUCCESS on success, error code on failure
 *
 * EXAMPLE:
 *   swarm_phoneme_message_t msg;
 *   float position[3] = {10.5f, 20.3f, 5.0f};
 *   swarm_protocol_encode(&msg, SWARM_MSG_TARGET_FOUND, 42, position, 3);
 */
nimcp_error_t swarm_protocol_encode(
    swarm_phoneme_message_t* msg,
    swarm_message_type_t type,
    uint16_t sender_id,
    const float* payload,
    uint32_t payload_len
);

/**
 * @brief Decode swarm message
 *
 * WHAT: Extract message type and payload from received message
 * WHY:  Parse incoming drone communications
 * HOW:  Validate CRC, extract fields from message structure
 *
 * ALGORITHM:
 * 1. Validate CRC16 checksum
 * 2. Extract message type
 * 3. Extract sender ID
 * 4. Copy payload to output buffer
 * 5. Return sequence length
 *
 * @param msg Input message to decode
 * @param type Output message type (can be NULL)
 * @param sender_id Output sender ID (can be NULL)
 * @param payload Output payload buffer (can be NULL)
 * @param payload_len Output payload length in floats (can be NULL)
 * @return NIMCP_SUCCESS on success, error code on failure
 *
 * EXAMPLE:
 *   swarm_message_type_t type;
 *   uint16_t sender;
 *   float payload[4];
 *   uint32_t len;
 *   if (swarm_protocol_decode(&msg, &type, &sender, payload, &len) == NIMCP_SUCCESS) {
 *       // Process message...
 *   }
 */
nimcp_error_t swarm_protocol_decode(
    const swarm_phoneme_message_t* msg,
    swarm_message_type_t* type,
    uint16_t* sender_id,
    float* payload,
    uint32_t* payload_len
);

/**
 * @brief Validate message CRC
 *
 * WHAT: Verify message integrity using CRC16 checksum
 * WHY:  Detect transmission errors and corruption
 * HOW:  Recalculate CRC and compare to stored value
 *
 * CRC ALGORITHM: CRC16-CCITT (polynomial 0x1021, init 0xFFFF)
 * - Used in XMODEM, Bluetooth, many RF protocols
 * - Detects all single-bit errors
 * - Detects all double-bit errors
 * - Detects 99.998% of longer burst errors
 *
 * @param msg Message to validate
 * @return true if CRC valid, false if corrupted
 *
 * EXAMPLE:
 *   if (!swarm_protocol_validate(&msg)) {
 *       printf("Corrupted message, discarding\n");
 *       return;
 *   }
 */
bool swarm_protocol_validate(const swarm_phoneme_message_t* msg);

/**
 * @brief Get phoneme sequence for message type
 *
 * WHAT: Retrieve standard phoneme sequence for a message type
 * WHY:  Enable phoneme-based message recognition
 * HOW:  Lookup table mapping type → phoneme sequence
 *
 * PHONEME MAPPINGS:
 * - HEARTBEAT:         /hɛlo/    (PHONEME_H, PHONEME_EH, PHONEME_L, PHONEME_OW)
 * - THREAT_DETECTED:   /deɪnʤər/ (PHONEME_D, PHONEME_EY, PHONEME_N, PHONEME_JH, PHONEME_ER)
 * - TARGET_FOUND:      /faʊnd/   (PHONEME_F, PHONEME_AO, PHONEME_N, PHONEME_D)
 * - REQUEST_BACKUP:    /hɛlp/    (PHONEME_H, PHONEME_EH, PHONEME_L, PHONEME_P)
 * - FORMATION_CHANGE:  /muv/     (PHONEME_M, PHONEME_UW, PHONEME_V)
 * - REWARD_SIGNAL:     /gʊd/     (PHONEME_G, PHONEME_UH, PHONEME_D)
 * - NEUROMOD_SYNC:     /sɪŋk/    (PHONEME_S, PHONEME_IH, PHONEME_NG, PHONEME_K)
 * - WORKSPACE_BROADCAST: /ʃɛr/   (PHONEME_SH, PHONEME_EH, PHONEME_R)
 * - VOTE_REQUEST:      /vot/     (PHONEME_V, PHONEME_OW, PHONEME_T)
 * - VOTE_RESPONSE:     /jɛs/     (PHONEME_Y, PHONEME_EH, PHONEME_S)
 * - SERVER_UPDATE:     /sɛt/     (PHONEME_S, PHONEME_EH, PHONEME_T)
 *
 * @param type Message type
 * @param phonemes Output phoneme sequence (must be pre-allocated, size ≥ 8)
 * @param length Output sequence length
 * @return NIMCP_SUCCESS on success, error code on failure
 *
 * EXAMPLE:
 *   uint8_t phonemes[8];
 *   uint32_t len;
 *   swarm_protocol_get_phonemes_for_type(SWARM_MSG_HEARTBEAT, phonemes, &len);
 *   // phonemes = {PHONEME_H, PHONEME_EH, PHONEME_L, PHONEME_OW}, len = 4
 */
nimcp_error_t swarm_protocol_get_phonemes_for_type(
    swarm_message_type_t type,
    uint8_t* phonemes,
    uint32_t* length
);

/**
 * @brief Recognize message type from phoneme sequence
 *
 * WHAT: Classify message type from phoneme sequence
 * WHY:  Enable phoneme-based message recognition (reverse of get_phonemes)
 * HOW:  Match phoneme sequence against known patterns
 *
 * @param phonemes Input phoneme sequence
 * @param length Sequence length
 * @param type Output message type
 * @return NIMCP_SUCCESS if recognized, NIMCP_ERROR_NOT_FOUND if unknown
 *
 * EXAMPLE:
 *   uint8_t phonemes[] = {PHONEME_H, PHONEME_EH, PHONEME_L, PHONEME_OW};
 *   swarm_message_type_t type;
 *   swarm_protocol_recognize_type(phonemes, 4, &type);
 *   // type = SWARM_MSG_HEARTBEAT
 */
nimcp_error_t swarm_protocol_recognize_type(
    const uint8_t* phonemes,
    uint32_t length,
    swarm_message_type_t* type
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get message type name as string
 *
 * @param type Message type
 * @return Human-readable name (e.g., "HEARTBEAT", "THREAT_DETECTED")
 */
const char* swarm_protocol_message_type_name(swarm_message_type_t type);

/**
 * @brief Get phoneme-based "word" for message type
 *
 * @param type Message type
 * @return Phonetic word (e.g., "HELLO", "DANGER", "FOUND")
 */
const char* swarm_protocol_message_type_word(swarm_message_type_t type);

/**
 * @brief Print message to stdout (debugging)
 *
 * WHAT: Human-readable message dump
 * WHY:  Debugging and logging
 * HOW:  Print type, sender, payload, phonemes
 *
 * @param msg Message to print
 */
void swarm_protocol_print_message(const swarm_phoneme_message_t* msg);

/**
 * @brief Get protocol statistics
 *
 * @param stats Output statistics structure
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t swarm_protocol_get_stats(swarm_protocol_stats_t* stats);

/**
 * @brief Reset protocol statistics
 */
void swarm_protocol_reset_stats(void);

//=============================================================================
// CRC16 Calculation (Internal but exposed for testing)
//=============================================================================

/**
 * @brief Calculate CRC16-CCITT checksum
 *
 * WHAT: Compute CRC16 for data buffer
 * WHY:  Error detection for message integrity
 * HOW:  CRC16-CCITT algorithm (polynomial 0x1021, init 0xFFFF)
 *
 * ALGORITHM:
 * - Polynomial: 0x1021 (x^16 + x^12 + x^5 + 1)
 * - Initial value: 0xFFFF
 * - No final XOR
 * - Used in XMODEM, Kermit, Bluetooth
 *
 * @param data Data buffer
 * @param length Buffer size in bytes
 * @return 16-bit CRC checksum
 *
 * COMPLEXITY: O(n) where n = length
 *
 * EXAMPLE:
 *   uint8_t data[] = {0x01, 0x02, 0x03};
 *   uint16_t crc = swarm_protocol_crc16(data, 3);
 */
uint16_t swarm_protocol_crc16(const uint8_t* data, uint32_t length);

//=============================================================================
// Payload Helpers for Common Message Types
//=============================================================================

/**
 * @brief Encode heartbeat message
 *
 * WHAT: Create heartbeat with position data
 * WHY:  Convenience function for common use case
 * HOW:  Encode SWARM_MSG_HEARTBEAT with [x, y, z, battery]
 *
 * @param msg Output message
 * @param sender_id Drone ID
 * @param x X position
 * @param y Y position
 * @param z Z position (altitude)
 * @param battery Battery level [0.0, 1.0]
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t swarm_protocol_encode_heartbeat(
    swarm_phoneme_message_t* msg,
    uint16_t sender_id,
    float x,
    float y,
    float z,
    float battery
);

/**
 * @brief Encode threat detection message
 *
 * WHAT: Report threat at specific location
 * WHY:  Alert swarm to danger
 * HOW:  Encode SWARM_MSG_THREAT_DETECTED with [x, y, z, confidence]
 *
 * @param msg Output message
 * @param sender_id Drone ID
 * @param x Threat X position
 * @param y Threat Y position
 * @param z Threat Z position
 * @param confidence Detection confidence [0.0, 1.0]
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t swarm_protocol_encode_threat(
    swarm_phoneme_message_t* msg,
    uint16_t sender_id,
    float x,
    float y,
    float z,
    float confidence
);

/**
 * @brief Encode target found message
 *
 * WHAT: Report target discovery
 * WHY:  Coordinate swarm toward objective
 * HOW:  Encode SWARM_MSG_TARGET_FOUND with [x, y, z, priority]
 *
 * @param msg Output message
 * @param sender_id Drone ID
 * @param x Target X position
 * @param y Target Y position
 * @param z Target Z position
 * @param priority Target priority [0.0, 1.0]
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t swarm_protocol_encode_target_found(
    swarm_phoneme_message_t* msg,
    uint16_t sender_id,
    float x,
    float y,
    float z,
    float priority
);

/**
 * @brief Encode vote request message
 *
 * WHAT: Request vote from swarm (consensus mechanism)
 * WHY:  Democratic decision-making
 * HOW:  Encode SWARM_MSG_VOTE_REQUEST with [proposal_id, option_count, deadline, 0]
 *
 * @param msg Output message
 * @param sender_id Drone ID
 * @param proposal_id Unique proposal identifier
 * @param option_count Number of vote options
 * @param deadline Voting deadline (timestamp)
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t swarm_protocol_encode_vote_request(
    swarm_phoneme_message_t* msg,
    uint16_t sender_id,
    float proposal_id,
    float option_count,
    float deadline
);

/**
 * @brief Encode vote response message
 *
 * WHAT: Cast vote in response to vote request
 * WHY:  Participate in swarm consensus
 * HOW:  Encode SWARM_MSG_VOTE_RESPONSE with [proposal_id, vote_choice, confidence, 0]
 *
 * @param msg Output message
 * @param sender_id Drone ID
 * @param proposal_id Proposal being voted on
 * @param vote_choice Vote selection (0-based option index)
 * @param confidence Vote confidence [0.0, 1.0]
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t swarm_protocol_encode_vote_response(
    swarm_phoneme_message_t* msg,
    uint16_t sender_id,
    float proposal_id,
    float vote_choice,
    float confidence
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_SWARM_PROTOCOL_H
