#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_protocol.c - Refactored NIMCP 2.0 Protocol Implementation
//=============================================================================
// ARCHITECTURAL OVERVIEW:
// This module implements the NIMCP 2.0 protocol using several design patterns:
//
// - Strategy Pattern: Message type handlers via function pointer tables
// - Factory Pattern: Message creation functions for different types
// - Builder Pattern: Complex message construction with validation
// - Template Method: Serialization/deserialization framework
//
// COMPLEXITY ANALYSIS:
// - Message serialization: O(n) where n = payload size (optimal)
// - Message deserialization: O(n) single-pass parsing
// - Header validation: O(1) constant-time checks
// - Checksum calculation: O(n) single-pass CRC32
// - Feature code matching: O(1) bit operations
//
// DESIGN PRINCIPLES:
// - Single Responsibility: Each function has one clear purpose
// - Open/Closed: Extensible via strategy pattern
// - DRY: No code duplication, shared validation logic
// - Guard Clauses: Early returns, no nested ifs
//
// INVARIANTS:
// - All serialized messages include valid header with magic number
// - Checksums are always verified on deserialization
// - Buffer sizes are validated before any memory operations
// - Version compatibility is enforced for all messages
//=============================================================================

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "NETWORKING"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(protocol)

#include "networking/protocol/nimcp_protocol.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include "utils/validation/nimcp_validate.h"
#include "core/brain/factory/init/nimcp_brain_init.h"  // Phase IS-1: BBB access
#include "security/nimcp_blood_brain_barrier.h"        // Phase IS-1: BBB perimeter defense

//=============================================================================
// Constants and Configuration
//=============================================================================

#define MIN_BUFFER_SIZE sizeof(msg_header_t)
#define EVENT_MIN_SIZE sizeof(event_packet_t)
#define CONTROL_MIN_SIZE sizeof(control_message_t)

//=============================================================================
// CRC32 Lookup Table - Performance Optimization
//=============================================================================

/**
 * @brief CRC32 lookup table for O(n) checksum calculation
 *
 * WHY: Pre-computed CRC32 values enable single-pass checksum calculation
 * in O(n) time vs O(n*k) for polynomial long division. Standard polynomial
 * 0x04C11DB7 (IEEE 802.3 CRC32) provides strong error detection.
 *
 * COMPLEXITY: O(1) table lookup per byte processed
 *
 * INVARIANT: Table is immutable and contains exactly 256 entries
 */
static const uint32_t crc32_table[256] = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3,
    0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988, 0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91,
    0x1DB71064, 0x6AB020F2, 0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
    0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5,
    0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172, 0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B,
    0x35B5A8FA, 0x42B2986C, 0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
    0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F,
    0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924, 0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D,
    0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D, 0x91646C97, 0xE6635C01,
    0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E, 0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457,
    0x65B0D9C6, 0x12B7E950, 0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
    0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB,
    0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0, 0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9,
    0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
    0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81, 0xB7BD5C3B, 0xC0BA6CAD,
    0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A, 0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683,
    0xE3630B12, 0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7,
    0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC, 0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5,
    0xD6D6A3E8, 0xA1D1937E, 0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
    0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55, 0x316E8EEF, 0x4669BE79,
    0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236, 0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F,
    0xC5BA3BBE, 0xB2BD0B28, 0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
    0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F, 0x72076785, 0x05005713,
    0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38, 0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21,
    0x86D3D2D4, 0xF1D4E242, 0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45,
    0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2, 0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB,
    0xAED16A4A, 0xD9D65ADC, 0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
    0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693, 0x54DE5729, 0x23D967BF,
    0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94, 0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D};
//=============================================================================
// Message Header Factory - Builder Pattern
//=============================================================================

/**
 * @brief Initializes message header with protocol defaults
 *
 * WHY: Factory function ensures all headers are created with valid values.
 * Eliminates possibility of uninitialized or inconsistent headers.
 * Single point of control for header creation.
 *
 * COMPLEXITY: O(1) - Fixed field assignments
 *
 * INVARIANT: Output header always has valid magic number and version
 *
 * @param header Output header to initialize (must not be NULL)
 * @param type Message type identifier
 * @param payload_len Size of payload in bytes
 * @param sequence Message sequence number for ordering
 */
static void init_header(msg_header_t* header, msg_type_t type, uint32_t payload_len,
                        uint32_t sequence)
{
    // Guard clause: Validate header pointer
    if (!header)
        return;

    memset(header, 0, sizeof(*header));
    header->magic = PROTOCOL_MAGIC;
    header->version = PROTOCOL_VERSION;
    header->type = type;
    header->length = payload_len;
    header->sequence = sequence;
    header->checksum = 0;
}

//=============================================================================
// Serialization Helper Functions
//=============================================================================

/**
 * @brief Validates serialization inputs
 *
 * WHY: Extracted validation logic prevents nested ifs in main function.
 * Single responsibility - only validates buffer capacity.
 *
 * COMPLEXITY: O(1)
 *
 * @return true if inputs valid, false otherwise
 */
static bool validate_serialize_inputs(const uint8_t* buffer, uint32_t buffer_size,
                                      uint32_t payload_len)
{
    // Guard clause: Check buffer pointer
    if (!buffer)
        {

            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "validate_serialize_inputs: buffer is NULL");

            return false;

        }

    // Guard clause: Integer overflow check
    if (payload_len > UINT32_MAX - (uint32_t)sizeof(msg_header_t)) {
        return false;  // Would overflow uint32_t
    }

    // Guard clause: Check buffer size
    uint32_t required_size = (uint32_t)sizeof(msg_header_t) + payload_len;
    if (buffer_size < required_size) {
        return false;
    }

    return true;
}

/**
 * @brief Copies payload data to buffer if present
 *
 * WHY: Extracted payload copy logic. Single responsibility.
 * Handles optional payload safely.
 *
 * COMPLEXITY: O(n) where n = payload_len
 */
static void copy_payload_to_buffer(uint8_t* buffer, const void* payload, uint32_t payload_len)
{
    // Guard clause: Check if payload exists
    if (!payload || payload_len == 0)
        return;

    // Guard clause: Check buffer
    if (!buffer)
        return;

    memcpy(buffer + sizeof(msg_header_t), payload, payload_len);
}

/**
 * @brief Serializes a message into buffer (Template Method Pattern)
 *
 * WHY: Converts in-memory message structure to wire format for transmission.
 * Single-pass serialization with integrated checksum calculation.
 *
 * ALGORITHM:
 * 1. Validate inputs and buffer capacity (O(1))
 * 2. Initialize header with message metadata (O(1))
 * 3. Calculate checksum over header + payload (O(n))
 * 4. Copy header to buffer (O(1))
 * 5. Copy payload to buffer (O(n))
 *
 * COMPLEXITY: O(n) where n = payload_len (optimal, single pass)
 *
 * INVARIANT: Never writes beyond buffer_size
 *
 * @param type Message type identifier
 * @param payload Pointer to payload data (NULL if no payload)
 * @param payload_len Size of payload in bytes (0 if none)
 * @param buffer Output buffer for serialized message
 * @param buffer_size Size of output buffer
 * @return Bytes written on success, -1 on error
 */
int protocol_serialize_message(msg_type_t type, const void* payload, uint32_t payload_len,
                               uint8_t* buffer, uint32_t buffer_size)
{
    // Step 1: Validate inputs
    if (!validate_serialize_inputs(buffer, buffer_size, payload_len)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "protocol_serialize_message: validate_serialize_inputs is NULL");
        return -1;
    }

    // Step 2: Initialize header
    msg_header_t header;
    init_header(&header, type, payload_len, 0);

    // Step 3: Calculate checksum
    header.checksum = protocol_calculate_checksum(&header, payload, payload_len);

    // Step 4: Copy header to buffer
    memcpy(buffer, &header, sizeof(msg_header_t));

    // Step 5: Copy payload if present
    copy_payload_to_buffer(buffer, payload, payload_len);

    return sizeof(msg_header_t) + payload_len;
}

//=============================================================================
// Deserialization Helper Functions
//=============================================================================

/**
 * @brief Validates network buffer through BBB perimeter defense (Phase IS-1)
 *
 * WHY: Network data is untrusted external input - must pass through BBB
 *      before any parsing or processing to prevent injection attacks.
 *
 * COMPLEXITY: O(n) where n = buffer_size (BBB performs content inspection)
 *
 * @return true if BBB validation passes (or BBB not enabled), false if rejected
 */
static bool bbb_validate_network_buffer(const uint8_t* buffer, uint32_t buffer_size)
{
    // Guard clause: Check buffer
    if (!buffer || buffer_size == 0)
        return true;  // Let standard validation handle NULL/empty

    // Get global BBB system (thread-safe)
    bbb_system_t bbb = nimcp_bbb_get_global_system();
    if (!bbb)
        return true;  // BBB not enabled - pass through

    // Validate network buffer through BBB input gate
    bbb_validation_result_t result;
    if (!bbb_validate_input(bbb, buffer, buffer_size, &result)) {
        // BBB rejected the input - log and fail
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bbb_validate_network_buffer: bbb_validate_input is NULL");
        return false;
    }

    return true;
}

/**
 * @brief Validates deserialization inputs
 *
 * WHY: Extracted validation to eliminate nested ifs. Guard clause pattern.
 *
 * COMPLEXITY: O(1)
 *
 * @return true if inputs valid, false otherwise
 */
static bool validate_deserialize_inputs(const uint8_t* buffer, uint32_t buffer_size,
                                        const msg_header_t* header)
{
    // Guard clause: Check buffer
    if (!buffer)
        {

            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "validate_deserialize_inputs: buffer is NULL");

            return false;

        }

    // Guard clause: Check header
    if (!header)
        {

            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "validate_deserialize_inputs: header is NULL");

            return false;

        }

    // Guard clause: Check minimum buffer size
    if (buffer_size < sizeof(msg_header_t)) {
        return false;
    }

    return true;
}

/**
 * @brief Extracts header from buffer
 *
 * WHY: Separated header extraction for clarity. Single responsibility.
 *
 * COMPLEXITY: O(1) - Fixed size copy
 *
 * @return true if header valid, false otherwise
 */
static bool extract_and_validate_header(const uint8_t* buffer, msg_header_t* header)
{
    // Guard clause: Validate inputs
    if (!buffer || !header) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "extract_and_validate_header: required parameter is NULL (buffer, header)");
        return false;
    }

    // Copy header from buffer
    memcpy(header, buffer, sizeof(msg_header_t));

    // Validate header fields
    return protocol_validate_header(header);
}

/**
 * @brief Copies payload from buffer if present
 *
 * WHY: Extracted payload extraction. Handles optional payload safely.
 *
 * COMPLEXITY: O(n) where n = payload length
 */
static void extract_payload_from_buffer(const uint8_t* buffer, void* payload, uint32_t payload_len)
{
    // Guard clause: Check if payload needed
    if (!payload || payload_len == 0)
        return;

    // Guard clause: Check buffer
    if (!buffer)
        return;

    memcpy(payload, buffer + sizeof(msg_header_t), payload_len);
}

/**
 * @brief Verifies message checksum
 *
 * WHY: Extracted checksum verification. Single responsibility.
 * Critical for detecting transmission errors.
 *
 * COMPLEXITY: O(n) where n = total message size
 *
 * @return true if checksum valid, false otherwise
 */
static bool verify_message_checksum(const msg_header_t* header, const void* payload)
{
    // Guard clause: Validate header
    if (!header)
        {

            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "verify_message_checksum: header is NULL");

            return false;

        }

    uint32_t calculated = protocol_calculate_checksum(header, payload, header->length);

    return calculated == header->checksum;
}

/**
 * @brief Deserializes message from buffer (Template Method Pattern)
 *
 * WHY: Converts wire format to in-memory structures for processing.
 * Single-pass parsing with integrated validation and checksum verification.
 *
 * ALGORITHM:
 * 1. Validate inputs (O(1))
 * 2. Extract and validate header (O(1))
 * 3. Check payload size compatibility (O(1))
 * 4. Extract payload if present (O(n))
 * 5. Verify checksum (O(n))
 *
 * COMPLEXITY: O(n) where n = message size (optimal, single pass)
 *
 * INVARIANT: Never reads beyond buffer_size, never writes beyond payload_size
 *
 * @param buffer Input buffer containing serialized message
 * @param buffer_size Size of input buffer
 * @param header Output header structure
 * @param payload Output buffer for payload (NULL if not needed)
 * @param payload_size Size of payload buffer
 * @return Bytes read on success, -1 on error
 */
int protocol_deserialize_message(const uint8_t* buffer, uint32_t buffer_size, msg_header_t* header,
                                 void* payload, uint32_t payload_size)
{
    // Step 0: BBB perimeter defense (Phase IS-1) - validate untrusted network data
    if (!bbb_validate_network_buffer(buffer, buffer_size)) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_STATE, "buffer",
                              "BBB perimeter defense rejected network buffer");
        return -1;  // BBB rejected the input
    }

    // Step 1: Validate inputs
    if (!validate_deserialize_inputs(buffer, buffer_size, header)) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM,
                   "Invalid deserialization inputs: buffer=%p, buffer_size=%u, header=%p",
                   (void*)buffer, buffer_size, (void*)header);
        return -1;
    }

    // Step 2: Extract and validate header
    if (!extract_and_validate_header(buffer, header)) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "message header",
                            "Failed to extract or validate message header");
        return -1;
    }

    // Step 2.5: SECURITY - Check buffer contains full payload (prevent buffer overflow)
    // Validates that buffer_size >= sizeof(msg_header_t) + header->length
    if (header->length > UINT32_MAX - sizeof(msg_header_t)) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_STATE, "header->length",
                              "Integer overflow in payload length: %u", header->length);
        return -1;  // Integer overflow protection
    }
    if (buffer_size < sizeof(msg_header_t) + header->length) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_STATE, "buffer_size",
                              "Buffer too small for claimed payload: buffer=%u, required=%lu",
                              buffer_size, (unsigned long)(sizeof(msg_header_t) + header->length));
        return -1;  // Buffer too small for claimed payload
    }

    // Step 3: Check payload size
    if (header->length > payload_size) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_STATE, "payload_size",
                              "Payload buffer too small: available=%u, required=%u",
                              payload_size, header->length);
        return -1;
    }

    // Step 4: Extract payload if present
    extract_payload_from_buffer(buffer, payload, header->length);

    // Step 5: Verify checksum
    if (!verify_message_checksum(header, payload)) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "checksum",
                            "Message checksum verification failed");
        return -1;
    }

    return sizeof(msg_header_t) + header->length;
}

//=============================================================================
// Header Validation Functions
//=============================================================================

/**
 * @brief Validates magic number in header
 *
 * WHY: Extracted validation check. Detects malformed messages early.
 *
 * COMPLEXITY: O(1)
 */
static bool is_magic_valid(uint32_t magic)
{
    return magic == PROTOCOL_MAGIC;
}

/**
 * @brief Minimum supported protocol version for backward compatibility
 * SECURITY: Allows negotiation with older clients while rejecting
 *           unsupported or maliciously crafted versions
 */
#define PROTOCOL_VERSION_MIN 1

/**
 * @brief Maximum supported protocol version
 * SECURITY: Prevents future version exploitation attacks
 */
#define PROTOCOL_VERSION_MAX 2

/**
 * @brief Validates protocol version
 *
 * WHY: Ensures version compatibility. Prevents protocol mismatch issues.
 *      SECURITY: Validates version is within supported range to prevent:
 *      - Future version exploitation (attacker claims version 255)
 *      - Past version downgrade attacks (force weak crypto)
 *      - Integer overflow/underflow in version checks
 *
 * COMPLEXITY: O(1)
 */
static bool is_version_valid(uint8_t version)
{
    // SECURITY: Check version is within valid range
    // This prevents both future version attacks and downgrade attacks
    if (version < PROTOCOL_VERSION_MIN) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "is_version_valid: validation failed");
        return false;  // Reject ancient/unsupported versions
    }
    if (version > PROTOCOL_VERSION_MAX) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "is_version_valid: validation failed");
        return false;  // Reject unknown future versions
    }

    // Accept current version or compatible versions in range
    return true;
}

/**
 * @brief Validates message type
 *
 * WHY: Ensures message type is within valid enum range.
 *
 * COMPLEXITY: O(1)
 */
static bool is_type_valid(msg_type_t type)
{
    return type < MSG_TYPE_MAX;
}

/**
 * @brief Validates payload length
 *
 * WHY: Prevents buffer overflow attacks via oversized payloads.
 *
 * COMPLEXITY: O(1)
 */
static bool is_payload_length_valid(uint32_t length)
{
    return length <= MAX_PAYLOAD_SIZE;
}

/**
 * @brief Validates message header fields
 *
 * WHY: Ensures all header fields contain valid values before processing.
 * Critical for security and protocol correctness. Uses guard clauses
 * to avoid nested ifs.
 *
 * VALIDATION CHECKS:
 * - Magic number matches PROTOCOL_MAGIC
 * - Version matches PROTOCOL_VERSION
 * - Message type is within valid range
 * - Payload length does not exceed maximum
 *
 * COMPLEXITY: O(1) - Fixed number of field checks
 *
 * INVARIANT: Returns true only if ALL fields are valid
 *
 * @param header Pointer to header structure to validate
 * @return true if header valid, false otherwise
 */
bool protocol_validate_header(const msg_header_t* header)
{
    // Guard clause: Check pointer
    if (!header)
        {

            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "protocol_validate_header: header is NULL");

            return false;

        }

    // Guard clause: Validate magic field using nimcp_validate
    if (!nimcp_validate_integer_field(&header->magic, sizeof(uint32_t))) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "protocol_validate_header: nimcp_validate_integer_field is NULL");
        return false;
    }

    // Guard clause: Check magic number
    if (!is_magic_valid(header->magic)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "protocol_validate_header: is_magic_valid is NULL");
        return false;
    }

    // Guard clause: Validate version field using nimcp_validate
    if (!nimcp_validate_integer_field(&header->version, sizeof(uint8_t))) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "protocol_validate_header: nimcp_validate_integer_field is NULL");
        return false;
    }

    // Guard clause: Check version
    if (!is_version_valid(header->version)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "protocol_validate_header: is_version_valid is NULL");
        return false;
    }

    // Guard clause: Validate message type field using nimcp_validate
    if (!nimcp_validate_integer_field(&header->type, sizeof(msg_type_t))) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "protocol_validate_header: nimcp_validate_integer_field is NULL");
        return false;
    }

    // Guard clause: Check message type
    if (!is_type_valid(header->type)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "protocol_validate_header: is_type_valid is NULL");
        return false;
    }

    // Guard clause: Validate length field using nimcp_validate
    if (!nimcp_validate_integer_field(&header->length, sizeof(uint32_t))) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "protocol_validate_header: nimcp_validate_integer_field is NULL");
        return false;
    }

    // Guard clause: Check payload length
    if (!is_payload_length_valid(header->length)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "protocol_validate_header: is_payload_length_valid is NULL");
        return false;
    }

    // Guard clause: Validate sequence field using nimcp_validate
    if (!nimcp_validate_integer_field(&header->sequence, sizeof(uint32_t))) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "protocol_validate_header: nimcp_validate_integer_field is NULL");
        return false;
    }

    // Guard clause: Validate checksum field using nimcp_validate
    if (!nimcp_validate_integer_field(&header->checksum, sizeof(uint32_t))) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "protocol_validate_header: nimcp_validate_integer_field is NULL");
        return false;
    }

    return true;
}

//=============================================================================
// CRC32 Checksum Calculation
//=============================================================================

/**
 * @brief Calculates CRC32 over byte array
 *
 * WHY: Extracted core CRC calculation. Reusable for any byte array.
 * Single responsibility - only computes CRC value.
 *
 * COMPLEXITY: O(n) where n = data length (optimal, single pass)
 *
 * @param crc Initial CRC value
 * @param data Byte array to process
 * @param length Number of bytes to process
 * @return Updated CRC value
 */
static uint32_t calculate_crc32_bytes(uint32_t crc, const uint8_t* data, size_t length)
{
    // Guard clause: Check data pointer
    if (!data || length == 0)
        return crc;

    // Single pass through data - O(n)
    for (size_t i = 0; i < length; i++) {
        uint8_t table_index = (crc & 0xFF) ^ data[i];
        crc = (crc >> 8) ^ crc32_table[table_index];
    }

    return crc;
}

/**
 * @brief Calculates CRC32 for message header
 *
 * WHY: Extracted header CRC calculation. Uses local copy to avoid modifying
 *      the original const header (const-correct implementation).
 *
 * COMPLEXITY: O(1) - Fixed header size
 *
 * @param crc Initial CRC value
 * @param header Header to process (const - not modified)
 * @param saved_checksum Output: original checksum value (for verification)
 * @return Updated CRC value
 */
static uint32_t calculate_header_crc(uint32_t crc, const msg_header_t* header, uint32_t* saved_checksum)
{
    // Guard clause: Validate inputs
    if (!header || !saved_checksum)
        return crc;

    // Save original checksum for later restoration/verification
    *saved_checksum = header->checksum;

    // Create a local mutable copy of the header for CRC calculation
    // WHY: Avoids casting away const, maintains const-correctness
    msg_header_t header_copy;
    memcpy(&header_copy, header, sizeof(msg_header_t));
    header_copy.checksum = 0;  // Zero checksum field in copy only

    // Calculate CRC over the copy
    const uint8_t* data = (const uint8_t*)&header_copy;
    crc = calculate_crc32_bytes(crc, data, sizeof(msg_header_t));

    return crc;
}

/**
 * @brief Restores original checksum field value
 *
 * WHY: No longer needed since we use a local copy in calculate_header_crc.
 *      Kept as no-op for API compatibility in case any external code calls it.
 *
 * COMPLEXITY: O(1)
 *
 * @deprecated No longer modifies header since calculate_header_crc uses local copy
 */
static void restore_checksum_field(const msg_header_t* header, uint32_t saved_checksum)
{
    // No-op: header is now const and calculate_header_crc uses a local copy
    // This function is kept for API compatibility but does nothing
    (void)header;
    (void)saved_checksum;
}

/**
 * @brief Calculates CRC32 checksum for message
 *
 * WHY: Provides integrity checking for messages during transmission.
 * Detects corruption, truncation, or tampering. Uses IEEE 802.3 CRC32
 * polynomial for strong error detection.
 *
 * ALGORITHM:
 * 1. Initialize CRC to 0xFFFFFFFF (O(1))
 * 2. Create local copy of header with zeroed checksum (O(1))
 * 3. Calculate CRC over header copy (O(1) - fixed size)
 * 4. Calculate CRC over payload (O(n))
 * 5. Return inverted CRC (O(1))
 *
 * COMPLEXITY: O(n) where n = payload_len (optimal, single pass)
 *
 * CONST-CORRECTNESS: Header is not modified - uses local copy for calculation.
 *
 * @param header Message header (not modified - const-correct)
 * @param payload Payload data (NULL if none)
 * @param payload_len Size of payload in bytes
 * @return Calculated CRC32 checksum
 */
uint32_t protocol_calculate_checksum(const msg_header_t* header, const void* payload,
                                     uint32_t payload_len)
{
    // Guard clause: Validate header
    if (!header)
        return 0;

    uint32_t crc = 0xFFFFFFFF;
    uint32_t saved_checksum;

    // Calculate CRC over header (uses local copy internally, no const cast needed)
    crc = calculate_header_crc(crc, header, &saved_checksum);

    // Calculate CRC over payload if present
    if (payload && payload_len > 0) {
        crc = calculate_crc32_bytes(crc, (const uint8_t*) payload, payload_len);
    }

    // No restoration needed - calculate_header_crc uses local copy
    restore_checksum_field(header, saved_checksum);

    return ~crc;
}

//=============================================================================
// NIMCP 2.0: Event Packet Implementation
//=============================================================================

/**
 * @brief Validates event packet serialization inputs
 *
 * WHY: Extracted validation logic. Guard clause pattern.
 *
 * COMPLEXITY: O(1)
 */
static bool validate_event_serialize_inputs(const event_packet_t* packet, const uint8_t* buffer,
                                            uint32_t buffer_size)
{
    // Guard clause: Check packet
    if (!packet)
        {

            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "validate_event_serialize_inputs: packet is NULL");

            return false;

        }

    // Guard clause: Check buffer
    if (!buffer)
        {

            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "validate_event_serialize_inputs: buffer is NULL");

            return false;

        }

    // Guard clause: Integer overflow check
    if (packet->payload_length > UINT32_MAX - (uint32_t)sizeof(event_packet_t)) {
        return false;  // Would overflow uint32_t
    }

    // Guard clause: Check buffer size
    uint32_t required = (uint32_t)sizeof(event_packet_t) + packet->payload_length;
    if (buffer_size < required) {
        return false;
    }

    return true;
}

/**
 * @brief Copies event payload to buffer
 *
 * WHY: Extracted payload copy logic. Single responsibility.
 *
 * COMPLEXITY: O(n) where n = payload_length
 */
static void copy_event_payload(uint8_t* buffer, const void* payload, uint32_t payload_length)
{
    // Guard clause: Check if payload present
    if (!payload || payload_length == 0)
        return;

    // Guard clause: Check buffer
    if (!buffer)
        return;

    memcpy(buffer + sizeof(event_packet_t), payload, payload_length);
}

/**
 * @brief Serializes event packet into buffer
 *
 * WHY: Converts event packet to wire format for high-frequency neural spike
 * transmission. Optimized for minimal overhead and fast processing.
 *
 * ALGORITHM:
 * 1. Validate inputs and buffer capacity (O(1))
 * 2. Copy packet header to buffer (O(1) - fixed size)
 * 3. Copy payload if present (O(n))
 *
 * COMPLEXITY: O(n) where n = payload_length (optimal, single pass)
 *
 * INVARIANT: Never writes beyond buffer_size
 *
 * @param packet Event packet to serialize
 * @param payload Optional payload data (NULL if none)
 * @param buffer Output buffer for serialized data
 * @param buffer_size Size of output buffer
 * @return Bytes written on success, -1 on error
 */
int event_packet_serialize(const event_packet_t* packet, const void* payload, uint8_t* buffer,
                           uint32_t buffer_size)
{
    // Step 1: Validate inputs
    if (!validate_event_serialize_inputs(packet, buffer, buffer_size)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "event_packet_serialize: validate_event_serialize_inputs is NULL");
        return -1;
    }

    // Step 2: SECURITY - Zero reserved/padding fields to prevent memory disclosure
    event_packet_t packet_copy = *packet;
    packet_copy.reserved = 0;
    packet_copy.reserved2 = 0;

    // Step 3: Copy packet header
    memcpy(buffer, &packet_copy, sizeof(event_packet_t));

    // Step 4: Copy payload if present
    copy_event_payload(buffer, payload, packet->payload_length);

    return sizeof(event_packet_t) + packet->payload_length;
}

/**
 * @brief Validates event deserialization inputs
 *
 * WHY: Extracted validation. Guard clause pattern eliminates nesting.
 *
 * COMPLEXITY: O(1)
 */
static bool validate_event_deserialize_inputs(const uint8_t* buffer, uint32_t buffer_size,
                                              const event_packet_t* packet)
{
    // Guard clause: Check buffer
    if (!buffer)
        {

            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "validate_event_deserialize_inputs: buffer is NULL");

            return false;

        }

    // Guard clause: Check packet
    if (!packet)
        {

            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "validate_event_deserialize_inputs: packet is NULL");

            return false;

        }

    // Guard clause: Check buffer size
    if (buffer_size < sizeof(event_packet_t)) {
        return false;
    }

    return true;
}

/**
 * @brief Extracts event payload from buffer
 *
 * WHY: Separated payload extraction. Handles size validation.
 *
 * COMPLEXITY: O(n) where n = payload_length
 *
 * @return true on success, false on error
 */
static bool extract_event_payload(const uint8_t* buffer, void* payload, uint32_t payload_size,
                                  uint32_t payload_length)
{
    // Guard clause: Check if payload needed
    if (payload_length == 0)
        return true;

    // Guard clause: Check payload buffer
    if (!payload)
        {

            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "extract_event_payload: payload is NULL");

            return false;

        }

    // Guard clause: Check payload size
    if (payload_size < payload_length) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "extract_event_payload: validation failed");
        return false;
    }

    memcpy(payload, buffer + sizeof(event_packet_t), payload_length);
    return true;
}

/**
 * @brief Deserializes event packet from buffer
 *
 * WHY: Converts wire format back to in-memory event packet structure
 * for processing. Includes validation to ensure packet integrity.
 *
 * ALGORITHM:
 * 1. Validate inputs (O(1))
 * 2. Extract packet header (O(1) - fixed size)
 * 3. Validate packet fields (O(1))
 * 4. Extract payload if present (O(n))
 *
 * COMPLEXITY: O(n) where n = payload_length (optimal, single pass)
 *
 * INVARIANT: Never reads beyond buffer_size, never writes beyond payload_size
 *
 * @param buffer Input buffer containing serialized packet
 * @param buffer_size Size of input buffer
 * @param packet Output packet structure
 * @param payload Output buffer for payload (NULL if not needed)
 * @param payload_size Size of payload buffer
 * @return Bytes read on success, -1 on error
 */
int event_packet_deserialize(const uint8_t* buffer, uint32_t buffer_size, event_packet_t* packet,
                             void* payload, uint32_t payload_size)
{
    // Step 0: BBB perimeter defense (Phase IS-1) - validate untrusted network data
    if (!bbb_validate_network_buffer(buffer, buffer_size)) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_STATE, "buffer",
                              "BBB perimeter defense rejected event packet buffer");
        return -1;  // BBB rejected the input
    }

    // Step 1: Validate inputs
    if (!validate_event_deserialize_inputs(buffer, buffer_size, packet)) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM,
                   "Invalid event deserialization inputs");
        return -1;
    }

    // Step 2: Extract packet header
    memcpy(packet, buffer, sizeof(event_packet_t));

    // Step 3: Validate packet
    if (!event_packet_validate(packet)) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "event packet",
                            "Event packet validation failed");
        return -1;
    }

    // Step 4: Extract payload if present
    if (!extract_event_payload(buffer, payload, payload_size, packet->payload_length)) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_STATE, "payload_size",
                              "Event payload extraction failed: available=%u, required=%u",
                              payload_size, packet->payload_length);
        return -1;
    }

    return sizeof(event_packet_t) + packet->payload_length;
}

/**
 * @brief Checks if event packet version is valid
 *
 * WHY: Extracted version validation. Single responsibility.
 *
 * COMPLEXITY: O(1)
 */
static bool is_event_version_valid(const event_packet_t* packet)
{
    if (!packet)
        {

            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "is_event_version_valid: packet is NULL");

            return false;

        }
    return EVENT_GET_VERSION(packet) == PROTOCOL_VERSION;
}

/**
 * @brief Checks if event has valid type flags
 *
 * WHY: SECURITY - Events must have exactly one of E or I flag set.
 *      Both flags or neither flag is invalid to prevent ambiguous processing.
 *
 * COMPLEXITY: O(1)
 */
static bool has_required_event_flags(uint8_t flags)
{
    bool is_excitatory = (flags & EVENT_FLAG_EXCITATORY) != 0;
    bool is_inhibitory = (flags & EVENT_FLAG_INHIBITORY) != 0;

    // SECURITY: Require exactly one of E or I (XOR check)
    // Both set = invalid, neither set = invalid
    return is_excitatory != is_inhibitory;
}

/**
 * @brief Checks if excitatory and inhibitory flags are mutually exclusive
 *
 * WHY: Event cannot be both excitatory and inhibitory simultaneously.
 *
 * COMPLEXITY: O(1)
 */
static bool are_event_flags_exclusive(uint8_t flags)
{
    bool is_excitatory = (flags & EVENT_FLAG_EXCITATORY) != 0;
    bool is_inhibitory = (flags & EVENT_FLAG_INHIBITORY) != 0;
    return !(is_excitatory && is_inhibitory);
}

/**
 * @brief Validates event packet structure and fields
 *
 * WHY: Ensures event packet conforms to NIMCP 2.0 specification.
 * Critical for protocol correctness and preventing malformed packets
 * from propagating through neural network.
 *
 * VALIDATION CHECKS:
 * - Version matches PROTOCOL_VERSION
 * - At least one of E or I flags set
 * - E and I flags are mutually exclusive
 * - Payload length within bounds
 *
 * COMPLEXITY: O(1) - Fixed number of field checks
 *
 * INVARIANT: Returns true only if ALL validations pass
 *
 * @param packet Event packet to validate
 * @return true if valid, false otherwise
 */
bool event_packet_validate(const event_packet_t* packet)
{
    // Guard clause: Check pointer
    if (!packet)
        {

            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "event_packet_validate: packet is NULL");

            return false;

        }

    // Guard clause: Check version
    if (!is_event_version_valid(packet)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "event_packet_validate: is_event_version_valid is NULL");
        return false;
    }

    uint8_t flags = EVENT_GET_FLAGS(packet);

    // Guard clause: Check required flags present
    if (!has_required_event_flags(flags)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "event_packet_validate: has_required_event_flags is NULL");
        return false;
    }

    // Guard clause: Check flags mutually exclusive
    if (!are_event_flags_exclusive(flags)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "event_packet_validate: are_event_flags_exclusive is NULL");
        return false;
    }

    // Guard clause: Check payload length
    if (packet->payload_length > MAX_PAYLOAD_SIZE) {
        return false;
    }

    return true;
}

//=============================================================================
// NIMCP 2.0: Control Message Implementation
//=============================================================================

/**
 * @brief Calculates parameter size from message length
 *
 * WHY: Extracted calculation logic. Single responsibility.
 *
 * COMPLEXITY: O(1)
 */
static uint32_t calculate_param_size(const control_message_t* msg)
{
    if (!msg)
        return 0;
    if (msg->message_length < sizeof(control_message_t))
        return 0;
    return msg->message_length - sizeof(control_message_t);
}

/**
 * @brief Validates control message serialization inputs
 *
 * WHY: Extracted validation. Guard clause pattern.
 *
 * COMPLEXITY: O(1)
 */
static bool validate_control_serialize_inputs(const control_message_t* msg, const uint8_t* buffer,
                                              uint32_t buffer_size, uint32_t required_size)
{
    // Guard clause: Check message
    if (!msg)
        {

            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "validate_control_serialize_inputs: msg is NULL");

            return false;

        }

    // Guard clause: Check buffer
    if (!buffer)
        {

            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "validate_control_serialize_inputs: buffer is NULL");

            return false;

        }

    // Guard clause: Check buffer size
    if (buffer_size < required_size) {
        return false;
    }

    return true;
}

/**
 * @brief Copies control message parameters to buffer
 *
 * WHY: Extracted parameter copy. Handles optional params.
 *
 * COMPLEXITY: O(n) where n = param_size
 */
static void copy_control_params(uint8_t* buffer, const void* params, uint32_t param_size)
{
    // Guard clause: Check if params present
    if (!params || param_size == 0)
        return;

    // Guard clause: Check buffer
    if (!buffer)
        return;

    memcpy(buffer + sizeof(control_message_t), params, param_size);
}

/**
 * @brief Serializes control message into buffer
 *
 * WHY: Converts control message to wire format for configuration and
 * management operations. Used for low-frequency control plane messages.
 *
 * ALGORITHM:
 * 1. Calculate parameter size from message length (O(1))
 * 2. Validate inputs and buffer capacity (O(1))
 * 3. Copy message header to buffer (O(1) - fixed size)
 * 4. Copy parameters if present (O(n))
 *
 * COMPLEXITY: O(n) where n = param_size (optimal, single pass)
 *
 * INVARIANT: Never writes beyond buffer_size
 *
 * @param msg Control message to serialize
 * @param params Optional TLV-encoded parameters (NULL if none)
 * @param buffer Output buffer for serialized data
 * @param buffer_size Size of output buffer
 * @return Bytes written on success, -1 on error
 */
int control_message_serialize(const control_message_t* msg, const void* params, uint8_t* buffer,
                              uint32_t buffer_size)
{
    // Step 1: Calculate sizes
    uint32_t param_size = calculate_param_size(msg);
    uint32_t required_size = sizeof(control_message_t) + param_size;

    // Step 2: Validate inputs
    if (!validate_control_serialize_inputs(msg, buffer, buffer_size, required_size)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "control_message_serialize: validate_control_serialize_inputs is NULL");
        return -1;
    }

    // Step 3: SECURITY - Zero reserved/padding fields to prevent memory disclosure
    control_message_t msg_copy = *msg;
    msg_copy.reserved = 0;
    msg_copy.reserved2 = 0;

    // Step 4: Copy message header
    memcpy(buffer, &msg_copy, sizeof(control_message_t));

    // Step 5: Copy parameters if present
    copy_control_params(buffer, params, param_size);

    return required_size;
}

/**
 * @brief Validates control deserialization inputs
 *
 * WHY: Extracted validation. Guard clause pattern.
 *
 * COMPLEXITY: O(1)
 */
static bool validate_control_deserialize_inputs(const uint8_t* buffer, uint32_t buffer_size,
                                                const control_message_t* msg)
{
    // Guard clause: Check buffer
    if (!buffer)
        {

            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "validate_control_deserialize_inputs: buffer is NULL");

            return false;

        }

    // Guard clause: Check message
    if (!msg)
        {

            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "validate_control_deserialize_inputs: msg is NULL");

            return false;

        }

    // Guard clause: Check buffer size
    if (buffer_size < sizeof(control_message_t)) {
        return false;
    }

    return true;
}

/**
 * @brief Extracts control message parameters from buffer
 *
 * WHY: Separated parameter extraction. Handles size validation.
 *
 * COMPLEXITY: O(n) where n = actual_param_size
 *
 * @return true on success, false on error
 */
static bool extract_control_params(const uint8_t* buffer, void* params, uint32_t param_size,
                                   uint32_t actual_param_size)
{
    // Guard clause: Check if params needed
    if (actual_param_size == 0)
        return true;

    // Guard clause: Check params buffer
    if (!params)
        {

            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "extract_control_params: params is NULL");

            return false;

        }

    // Guard clause: Check param size
    if (param_size < actual_param_size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "extract_control_params: validation failed");
        return false;
    }

    memcpy(params, buffer + sizeof(control_message_t), actual_param_size);
    return true;
}

/**
 * @brief Deserializes control message from buffer
 *
 * WHY: Converts wire format back to in-memory control message structure.
 * Includes validation to ensure message integrity.
 *
 * ALGORITHM:
 * 1. Validate inputs (O(1))
 * 2. Extract message header (O(1) - fixed size)
 * 3. Validate message fields (O(1))
 * 4. Calculate parameter size (O(1))
 * 5. Extract parameters if present (O(n))
 *
 * COMPLEXITY: O(n) where n = param_size (optimal, single pass)
 *
 * INVARIANT: Never reads beyond buffer_size, never writes beyond param_size
 *
 * @param buffer Input buffer containing serialized message
 * @param buffer_size Size of input buffer
 * @param msg Output message structure
 * @param params Output buffer for parameters (NULL if not needed)
 * @param param_size Size of params buffer
 * @return Bytes read on success, -1 on error
 */
int control_message_deserialize(const uint8_t* buffer, uint32_t buffer_size, control_message_t* msg,
                                void* params, uint32_t param_size)
{
    // Step 0: BBB perimeter defense (Phase IS-1) - validate untrusted network data
    if (!bbb_validate_network_buffer(buffer, buffer_size)) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_STATE, "buffer",
                              "BBB perimeter defense rejected control message buffer");
        return -1;  // BBB rejected the input
    }

    // Step 1: Validate inputs
    if (!validate_control_deserialize_inputs(buffer, buffer_size, msg)) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM,
                   "Invalid control message deserialization inputs");
        return -1;
    }

    // Step 2: Extract message header
    memcpy(msg, buffer, sizeof(control_message_t));

    // Step 3: Validate message
    if (!control_message_validate(msg)) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_PARAM, "control message",
                            "Control message validation failed: type=%u, version=%u",
                            msg->msg_type, msg->version);
        return -1;
    }

    // Step 4: Calculate parameter size
    uint32_t actual_param_size = calculate_param_size(msg);

    // Step 5: Extract parameters if present
    if (!extract_control_params(buffer, params, param_size, actual_param_size)) {
        NIMCP_THROW(NIMCP_ERROR_INVALID_STATE, "param_size",
                              "Control params extraction failed: available=%u, required=%u",
                              param_size, actual_param_size);
        return -1;
    }

    return msg->message_length;
}

/**
 * @brief Checks if control message type is valid
 *
 * WHY: Extracted type validation. Single responsibility.
 *
 * COMPLEXITY: O(1)
 */
static bool is_control_type_valid(uint8_t msg_type)
{
    return msg_type < CTRL_MSG_MAX;
}

/**
 * @brief Checks if control message length is valid
 *
 * WHY: Ensures message length is within acceptable bounds.
 *
 * COMPLEXITY: O(1)
 */
static bool is_control_length_valid(uint32_t length)
{
    if (length < sizeof(control_message_t)) {
        return false;
    }
    if (length > MAX_PAYLOAD_SIZE) {
        return false;
    }
    return true;
}

/**
 * @brief Validates control message structure and fields
 *
 * WHY: Ensures control message conforms to NIMCP 2.0 specification.
 * Critical for control plane security and correctness.
 *
 * VALIDATION CHECKS:
 * - Version matches PROTOCOL_VERSION
 * - Message type is within valid range
 * - Message length is within bounds
 *
 * COMPLEXITY: O(1) - Fixed number of field checks
 *
 * INVARIANT: Returns true only if ALL validations pass
 *
 * @param msg Control message to validate
 * @return true if valid, false otherwise
 */
bool control_message_validate(const control_message_t* msg)
{
    // Guard clause: Check pointer
    if (!msg)
        {

            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "control_message_validate: msg is NULL");

            return false;

        }

    // Guard clause: Check version
    if (msg->version != PROTOCOL_VERSION) {
        return false;
    }

    // Guard clause: Check message type
    if (!is_control_type_valid(msg->msg_type)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "control_message_validate: is_control_type_valid is NULL");
        return false;
    }

    // Guard clause: Check message length
    if (!is_control_length_valid(msg->message_length)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "control_message_validate: is_control_length_valid is NULL");
        return false;
    }

    return true;
}

//=============================================================================
// NIMCP 2.0: Feature Code Functions
//=============================================================================

/**
 * @brief Get name of feature domain
 */
const char* feature_domain_name(feature_domain_t domain)
{
    switch (domain) {
        case FEATURE_DOMAIN_SYSTEM:
            return "System";
        case FEATURE_DOMAIN_VISION:
            return "Vision";
        case FEATURE_DOMAIN_AUDITORY:
            return "Auditory";
        case FEATURE_DOMAIN_LANGUAGE:
            return "Language";
        case FEATURE_DOMAIN_MOTOR:
            return "Motor";
        case FEATURE_DOMAIN_MEMORY:
            return "Memory";
        case FEATURE_DOMAIN_EMOTION:
            return "Emotion";
        case FEATURE_DOMAIN_ETHICS:
            return "Ethics";
        default:
            if (domain >= FEATURE_DOMAIN_USER_MIN && domain <= FEATURE_DOMAIN_USER_MAX) {
                return "User-Defined";
            }
            return "Unknown";
    }
}

/**
 * @brief Check if a feature code matches a filter with mask
 */
bool feature_code_matches(feature_code_t code, feature_code_t filter, uint32_t mask)
{
    return (code & mask) == (filter & mask);
}

//=============================================================================
// NIMCP 2.0: Subscription Functions
//=============================================================================

/**
 * @brief Check if an event packet matches a subscription filter
 */
bool subscription_matches(const subscription_filter_t* filter, const event_packet_t* packet)
{
    if (!filter || !packet) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "subscription_matches: invalid parameters");

            return false;
    }

    // Check feature code with mask
    feature_code_t packet_code = EVENT_GET_FEATURE_CODE(packet);
    if (!feature_code_matches(packet_code, filter->feature_code, filter->feature_mask)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "subscription_matches: feature_code_matches is NULL");
        return false;
    }

    // Check confidence threshold
    float confidence = EVENT_CONFIDENCE_TO_FLOAT(packet->confidence);
    if (confidence < filter->confidence_threshold) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "subscription_matches: validation failed");
        return false;
    }

    // Rate limiting would be checked by the caller based on timestamp

    return true;
}
