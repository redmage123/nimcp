#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_neural_language.c - Neural Language Implementation
//=============================================================================
/**
 * @file nimcp_neural_language.c
 * @brief Bandwidth-efficient semantic language for brain-to-brain communication
 *
 * WHAT: Implementation of 256 cognitive primitives + PAD emotions
 * WHY:  Enable meaningful communication under bandwidth/jamming constraints
 * HOW:  Expression builder, serialization, validation, interpretation
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include "networking/nlp/nimcp_neural_language.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "security/nimcp_bbb_helpers.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_wiring_helpers.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <stdio.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_buffer_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(neural_language)

#define NLANG_MODULE "neural_language"

//=============================================================================
// Constants
//=============================================================================

#define NLANG_VERSION 1
#define NLANG_CHECKSUM_POLY 0x8005  // CRC-16 polynomial

//=============================================================================
// Primitive Name Tables
//=============================================================================

static const char* percept_names[] = {
    "NULL", "SEE", "HEAR", "TOUCH", "SMELL", "TASTE", "TEMPERATURE", "PRESSURE",
    "MOTION", "PROXIMITY", "OBSTACLE", "TARGET", "THREAT", "FRIENDLY", "UNKNOWN", "SIGNAL"
};

static const char* action_names[] = {
    "STOP", "MOVE", "TURN", "ASCEND", "DESCEND", "HOVER", "FOLLOW", "EVADE",
    "APPROACH", "RETREAT", "ORBIT", "SCAN", "ACQUIRE", "RELEASE", "DEPLOY", "RETURN"
};

static const char* spatial_names[] = {
    "HERE", "THERE", "NEAR", "FAR", "ABOVE", "BELOW", "LEFT", "RIGHT",
    "FRONT", "BEHIND", "INSIDE", "OUTSIDE", "CENTER", "EDGE", "PATH", "ZONE"
};

static const char* temporal_names[] = {
    "NOW", "THEN", "BEFORE", "AFTER", "DURING", "UNTIL", "SINCE", "ALWAYS",
    "NEVER", "SOON", "LATER", "IMMEDIATE", "INTERVAL", "CYCLE", "DEADLINE", "DURATION"
};

static const char* quantity_names[] = {
    "ZERO", "ONE", "FEW", "SEVERAL", "MANY", "ALL", "SOME", "NONE",
    "MORE", "LESS", "HALF", "MOST", "MINIMUM", "MAXIMUM", "AVERAGE", "EXACT"
};

static const char* social_names[] = {
    "SELF", "OTHER", "GROUP", "LEADER", "FOLLOWER", "ALLY", "ENEMY", "NEUTRAL",
    "CIVILIAN", "VICTIM", "RESCUER", "TEAM_ALPHA", "TEAM_BETA", "TEAM_GAMMA", "BROADCAST", "MASTER"
};

static const char* cognitive_names[] = {
    "KNOW", "BELIEVE", "DOUBT", "UNKNOWN", "EXPECT", "REMEMBER", "FORGET", "LEARN",
    "RECOGNIZE", "DECIDE", "PLAN", "PREDICT", "CONFUSED", "ALERT", "FOCUS", "DISTRACTED"
};

static const char* emotion_names[] = {
    "NEUTRAL", "HAPPY", "SAD", "ANGRY", "AFRAID", "SURPRISED", "DISGUSTED", "CURIOUS",
    "FRUSTRATED", "CONFIDENT", "CAUTIOUS", "URGENT", "CALM", "STRESSED", "HOPEFUL", "DESPERATE"
};

static const char* intent_names[] = {
    "INFORM", "REQUEST", "COMMAND", "SUGGEST", "WARN", "PROMISE", "REFUSE", "ACCEPT",
    "ACKNOWLEDGE", "QUERY", "REPORT", "NEGOTIATE", "COORDINATE", "DELEGATE", "ESCALATE", "CANCEL"
};

static const char* query_names[] = {
    "WHAT", "WHERE", "WHEN", "WHO", "WHY", "HOW", "HOWMANY", "WHICH",
    "STATUS", "READY", "CAPABLE", "SAFE", "PRIORITY", "CONFIRM", "REPEAT", "ELABORATE"
};

static const char* assert_names[] = {
    "YES", "NO", "MAYBE", "TRUE", "FALSE", "POSSIBLE", "IMPOSSIBLE", "LIKELY",
    "UNLIKELY", "CONFIRMED", "UNCONFIRMED", "SUCCESS", "FAILURE", "PARTIAL", "COMPLETE", "ONGOING"
};

static const char* modifier_names[] = {
    "VERY", "SLIGHTLY", "NOT", "ALSO", "ONLY", "MAYBE", "DEFINITELY", "PROBABLY",
    "QUICKLY", "SLOWLY", "CAREFULLY", "QUIETLY", "LOUDLY", "REPEATEDLY", "ALTERNATIVELY", "CONDITIONALLY"
};

static const char* reference_names[] = {
    "THIS", "THAT", "PREVIOUS", "NEXT", "FIRST", "LAST", "SAME", "DIFFERENT",
    "CONTEXT_1", "CONTEXT_2", "CONTEXT_3", "CONTEXT_4", "TARGET_A", "TARGET_B", "WAYPOINT", "MISSION"
};

static const char* domain_names[] = {
    "FIRE", "FLOOD", "COLLAPSE", "CHEMICAL", "RADIATION", "MEDICAL",
    "TRIAGE_GREEN", "TRIAGE_YELLOW", "TRIAGE_RED", "TRIAGE_BLACK",
    "EVACUATION", "SHELTER", "SUPPLIES", "ROUTE_CLEAR", "ROUTE_BLOCKED", "LZ"
};

static const char* meta_names[] = {
    "BEGIN", "END", "SEQUENCE", "PARALLEL", "CONDITION", "CONSEQUENCE", "ALTERNATIVE", "LOOP",
    "REFERENCE", "DEFINE", "EXTEND", "LITERAL", "COMPRESS", "CHECKSUM", "VERSION", "RESERVED"
};

static const char* extended_names[] = {
    "WEATHER_CLEAR", "WEATHER_RAIN", "WEATHER_WIND", "WEATHER_FOG",
    "BATTERY_FULL", "BATTERY_LOW", "BATTERY_CRITICAL", "SENSOR_OFFLINE",
    "COMMS_DEGRADED", "COMMS_LOST", "GPS_LOCK", "GPS_LOST",
    "PAYLOAD_READY", "PAYLOAD_DEPLOYED", "PAYLOAD_EMPTY", "ESCAPE"
};

static const char* category_names[] = {
    "PERCEPT", "ACTION", "SPATIAL", "TEMPORAL", "QUANTITY", "SOCIAL",
    "COGNITIVE", "EMOTION", "INTENT", "QUERY", "ASSERT", "MODIFIER",
    "REFERENCE", "DOMAIN", "META", "EXTENDED"
};

//=============================================================================
// Expression Builder Implementation
//=============================================================================

void nlang_expr_init(nlang_expression_t* expr, nlang_intent_t intent) {
    if (!expr) return;

    memset(expr, 0, sizeof(nlang_expression_t));
    expr->version = NLANG_VERSION;
    expr->intent = (uint8_t)intent;
    expr->emotion = NLANG_PAD_NEUTRAL;
    expr->has_coord = NLANG_COORD_NONE;

    NIMCP_LOGGING_DEBUG("nlang", "Expression initialized with intent 0x%02X", intent);
}

int nlang_expr_add(nlang_expression_t* expr, uint8_t primitive) {
    if (!expr) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "expr is NULL");

        return -1;

    }

    if (expr->primitive_count >= NLANG_MAX_EXPRESSION_LEN) {
        NIMCP_LOGGING_WARN("nlang", "Expression full, cannot add primitive 0x%02X", primitive);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "nlang_expr_add: capacity exceeded");
        return -1;
    }

    expr->primitives[expr->primitive_count++] = primitive;
    return 0;
}

int nlang_expr_add_sequence(nlang_expression_t* expr,
                            const uint8_t* primitives,
                            size_t count) {
    if (!expr || !primitives) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nlang_expr_add: required parameter is NULL (expr, primitives)");
        return -1;
    }

    if (expr->primitive_count + count > NLANG_MAX_EXPRESSION_LEN) {
        NIMCP_LOGGING_WARN("nlang", "Not enough space for %zu primitives", count);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nlang_expr_add: validation failed");
        return -1;
    }

    memcpy(&expr->primitives[expr->primitive_count], primitives, count);
    expr->primitive_count += (uint8_t)count;
    return 0;
}

void nlang_expr_set_emotion(nlang_expression_t* expr, nlang_pad_emotion_t emotion) {
    if (!expr) return;
    expr->emotion = emotion;
}

void nlang_expr_set_coord_short(nlang_expression_t* expr,
                                int16_t lat_meters,
                                int16_t lon_meters) {
    if (!expr) return;

    expr->has_coord = NLANG_COORD_SHORT;
    expr->coord.short_coord.lat_offset = lat_meters;
    expr->coord.short_coord.lon_offset = lon_meters;
}

void nlang_expr_set_coord_3d(nlang_expression_t* expr,
                             int16_t lat_meters,
                             int16_t lon_meters,
                             int16_t alt_meters) {
    if (!expr) return;

    expr->has_coord = NLANG_COORD_3D;
    expr->coord.coord_3d.lat_offset = lat_meters;
    expr->coord.coord_3d.lon_offset = lon_meters;
    expr->coord.coord_3d.altitude = alt_meters;
}

void nlang_expr_set_coord_full(nlang_expression_t* expr,
                               double lat_deg,
                               double lon_deg) {
    if (!expr) return;

    expr->has_coord = NLANG_COORD_FULL;
    expr->coord.full_coord.latitude = (int32_t)(lat_deg * 1e7);
    expr->coord.full_coord.longitude = (int32_t)(lon_deg * 1e7);
}

void nlang_expr_add_context_ref(nlang_expression_t* expr, uint8_t slot) {
    if (!expr || slot >= NLANG_CONTEXT_SLOTS) return;
    expr->context_refs |= (1 << slot);
}

void nlang_expr_finalize(nlang_expression_t* expr) {
    if (!expr) return;
    expr->semantic_checksum = nlang_compute_semantic_checksum(expr);
    NIMCP_LOGGING_DEBUG("nlang", "Expression finalized: %u primitives, checksum 0x%04X",
                    expr->primitive_count, expr->semantic_checksum);
}

//=============================================================================
// Serialization Implementation
//=============================================================================

int nlang_expr_serialize(const nlang_expression_t* expr,
                         uint8_t* buffer,
                         size_t buffer_len) {
    if (!expr || !buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nlang_expr_finalize: required parameter is NULL (expr, buffer)");
        return -1;
    }

    size_t required = nlang_expr_size(expr);
    if (buffer_len < required) {
        NIMCP_LOGGING_WARN("nlang", "Buffer too small: %zu < %zu", buffer_len, required);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nlang_expr_finalize: validation failed");
        return -1;
    }

    size_t offset = 0;

    // Header: version, intent, primitive_count
    buffer[offset++] = expr->version;
    buffer[offset++] = expr->intent;
    buffer[offset++] = expr->primitive_count;

    // Primitives
    memcpy(&buffer[offset], expr->primitives, expr->primitive_count);
    offset += expr->primitive_count;

    // Emotion (3 bytes)
    buffer[offset++] = (uint8_t)expr->emotion.pleasure;
    buffer[offset++] = (uint8_t)expr->emotion.arousal;
    buffer[offset++] = (uint8_t)expr->emotion.dominance;

    // Coordinate flag and data
    buffer[offset++] = expr->has_coord;
    switch (expr->has_coord) {
        case NLANG_COORD_SHORT:
            memcpy(&buffer[offset], &expr->coord.short_coord, sizeof(nlang_short_coord_t));
            offset += sizeof(nlang_short_coord_t);
            break;
        case NLANG_COORD_3D:
            memcpy(&buffer[offset], &expr->coord.coord_3d, sizeof(nlang_coord_3d_t));
            offset += sizeof(nlang_coord_3d_t);
            break;
        case NLANG_COORD_FULL:
            memcpy(&buffer[offset], &expr->coord.full_coord, sizeof(nlang_full_coord_t));
            offset += sizeof(nlang_full_coord_t);
            break;
        default:
            break;
    }

    // Context refs
    buffer[offset++] = expr->context_refs;

    // Checksum (big-endian)
    buffer[offset++] = (uint8_t)(expr->semantic_checksum >> 8);
    buffer[offset++] = (uint8_t)(expr->semantic_checksum & 0xFF);

    return (int)offset;
}

int nlang_expr_deserialize(const uint8_t* buffer,
                           size_t buffer_len,
                           nlang_expression_t* expr) {
    if (!buffer || !expr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nlang_expr_finalize: required parameter is NULL (buffer, expr)");
        return -1;
    }

    // Minimum size: header(3) + emotion(3) + coord_flag(1) + context(1) + checksum(2)
    if (buffer_len < 10) {
        NIMCP_LOGGING_WARN("nlang", "Buffer too small for expression header");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nlang_expr_finalize: validation failed");
        return -1;
    }

    memset(expr, 0, sizeof(nlang_expression_t));
    size_t offset = 0;

    // Header
    expr->version = buffer[offset++];
    if (expr->version != NLANG_VERSION) {
        NIMCP_LOGGING_WARN("nlang", "Unsupported version: %u", expr->version);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nlang_expr_finalize: validation failed");
        return -1;
    }

    expr->intent = buffer[offset++];
    expr->primitive_count = buffer[offset++];

    if (expr->primitive_count > NLANG_MAX_EXPRESSION_LEN) {
        NIMCP_LOGGING_WARN("nlang", "Invalid primitive count: %u", expr->primitive_count);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nlang_expr_finalize: validation failed");
        return -1;
    }

    // Primitives
    if (offset + expr->primitive_count > buffer_len) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nlang_expr_finalize: validation failed");
        return -1;
    }
    memcpy(expr->primitives, &buffer[offset], expr->primitive_count);
    offset += expr->primitive_count;

    // Emotion
    if (offset + 3 > buffer_len) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nlang_expr_finalize: validation failed");
        return -1;
    }
    expr->emotion.pleasure = (int8_t)buffer[offset++];
    expr->emotion.arousal = (int8_t)buffer[offset++];
    expr->emotion.dominance = (int8_t)buffer[offset++];

    // Coordinate flag
    if (offset >= buffer_len) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nlang_expr_finalize: capacity exceeded");
        return -1;
    }
    expr->has_coord = buffer[offset++];

    // Coordinate data
    switch (expr->has_coord) {
        case NLANG_COORD_SHORT:
            if (offset + sizeof(nlang_short_coord_t) > buffer_len) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nlang_expr_finalize: validation failed");
                return -1;
            }
            memcpy(&expr->coord.short_coord, &buffer[offset], sizeof(nlang_short_coord_t));
            offset += sizeof(nlang_short_coord_t);
            break;
        case NLANG_COORD_3D:
            if (offset + sizeof(nlang_coord_3d_t) > buffer_len) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nlang_expr_finalize: validation failed");
                return -1;
            }
            memcpy(&expr->coord.coord_3d, &buffer[offset], sizeof(nlang_coord_3d_t));
            offset += sizeof(nlang_coord_3d_t);
            break;
        case NLANG_COORD_FULL:
            if (offset + sizeof(nlang_full_coord_t) > buffer_len) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nlang_expr_finalize: validation failed");
                return -1;
            }
            memcpy(&expr->coord.full_coord, &buffer[offset], sizeof(nlang_full_coord_t));
            offset += sizeof(nlang_full_coord_t);
            break;
        default:
            break;
    }

    // Context refs
    if (offset >= buffer_len) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nlang_expr_finalize: capacity exceeded");
        return -1;
    }
    expr->context_refs = buffer[offset++];

    // Checksum
    if (offset + 2 > buffer_len) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nlang_expr_finalize: validation failed");
        return -1;
    }
    expr->semantic_checksum = ((uint16_t)buffer[offset] << 8) | buffer[offset + 1];
    offset += 2;

    return (int)offset;
}

size_t nlang_expr_size(const nlang_expression_t* expr) {
    if (!expr) return 0;

    size_t size = 3;  // version, intent, primitive_count
    size += expr->primitive_count;  // primitives
    size += 3;  // emotion
    size += 1;  // coord flag

    switch (expr->has_coord) {
        case NLANG_COORD_SHORT: size += sizeof(nlang_short_coord_t); break;
        case NLANG_COORD_3D:    size += sizeof(nlang_coord_3d_t); break;
        case NLANG_COORD_FULL:  size += sizeof(nlang_full_coord_t); break;
        default: break;
    }

    size += 1;  // context_refs
    size += 2;  // checksum

    return size;
}

//=============================================================================
// Validation Implementation
//=============================================================================

bool nlang_expr_validate(const nlang_expression_t* expr) {
    if (!expr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nlang_expr_validate: expr is NULL");
        return false;
    }

    // Version check
    if (expr->version != NLANG_VERSION) {
        NIMCP_LOGGING_DEBUG("nlang", "Invalid version: %u", expr->version);
        return false;
    }

    // Primitive count check
    if (expr->primitive_count > NLANG_MAX_EXPRESSION_LEN) {
        NIMCP_LOGGING_DEBUG("nlang", "Invalid primitive count: %u", expr->primitive_count);
        return false;
    }

    // Intent validation (must be in intent category)
    if ((expr->intent & 0xF0) != 0x80) {
        NIMCP_LOGGING_DEBUG("nlang", "Invalid intent: 0x%02X", expr->intent);
        return false;
    }

    // Coordinate type validation
    if (expr->has_coord > NLANG_COORD_FULL) {
        NIMCP_LOGGING_DEBUG("nlang", "Invalid coord type: %u", expr->has_coord);
        return false;
    }

    return true;
}

bool nlang_expr_verify_checksum(const nlang_expression_t* expr) {
    if (!expr) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nlang_expr_verify_checksum: expr is NULL");
        return false;
    }

    uint16_t computed = nlang_compute_semantic_checksum(expr);
    return computed == expr->semantic_checksum;
}

uint16_t nlang_compute_semantic_checksum(const nlang_expression_t* expr) {
    if (!expr) return 0;

    // Semantic checksum considers:
    // 1. Intent (primary meaning)
    // 2. Primitive categories used (not specific values for ordering independence)
    // 3. Emotion quadrant (not exact values)
    // 4. Coordinate presence

    uint16_t hash = 0x1234;  // Seed

    // Include intent
    hash ^= ((uint16_t)expr->intent << 8);

    // Include primitive category distribution
    uint16_t category_mask = 0;
    for (uint8_t i = 0; i < expr->primitive_count; i++) {
        uint8_t cat = (expr->primitives[i] >> 4) & 0x0F;
        category_mask |= (1 << cat);
    }
    hash ^= category_mask;

    // Include emotion quadrant (sign bits only for semantic equivalence)
    uint8_t emotion_quadrant = 0;
    if (expr->emotion.pleasure > 0) emotion_quadrant |= 0x01;
    if (expr->emotion.arousal > 0)  emotion_quadrant |= 0x02;
    if (expr->emotion.dominance > 0) emotion_quadrant |= 0x04;
    hash ^= (emotion_quadrant << 4);

    // Include coordinate presence
    hash ^= ((uint16_t)expr->has_coord << 12);

    // Simple hash mixing
    hash = ((hash << 5) | (hash >> 11)) ^ 0xA5A5;
    hash ^= expr->primitive_count;
    hash = ((hash << 3) | (hash >> 13)) ^ 0x5A5A;

    return hash;
}

//=============================================================================
// Context Management Implementation
//=============================================================================

// Module initialization flag
static bool g_nlang_module_registered = false;
static bio_module_context_t g_nlang_bio_ctx = NULL;

/**
 * @brief Broadcast neural encoding event
 */
static void nlang_broadcast_encode_event(uint32_t context_id, uint32_t output_size,
                                          float confidence, bool success) {
    if (!g_nlang_bio_ctx) return;

    bio_msg_nlp_neural_complete_t msg;
    memset(&msg, 0, sizeof(msg));
    bio_msg_init_header(&msg.header, BIO_MSG_NLP_NEURAL_ENCODE_COMPLETE,
                        BIO_MODULE_NLP, BIO_MODULE_ALL, sizeof(msg));
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.channel = BIO_CHANNEL_ACETYLCHOLINE;  // Fast signaling
    msg.context_id = context_id;
    msg.output_size = output_size;
    msg.confidence = confidence;
    msg.success = success;
    msg.neurons_activated = output_size;  // Simplified mapping

    bio_router_broadcast(g_nlang_bio_ctx, &msg, sizeof(msg));
}

/**
 * @brief Broadcast neural decoding event
 */
static void nlang_broadcast_decode_event(uint32_t context_id, uint32_t output_size,
                                          float confidence, bool success) {
    if (!g_nlang_bio_ctx) return;

    bio_msg_nlp_neural_complete_t msg;
    memset(&msg, 0, sizeof(msg));
    bio_msg_init_header(&msg.header, BIO_MSG_NLP_NEURAL_DECODE_COMPLETE,
                        BIO_MODULE_NLP, BIO_MODULE_ALL, sizeof(msg));
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.channel = BIO_CHANNEL_ACETYLCHOLINE;
    msg.context_id = context_id;
    msg.output_size = output_size;
    msg.confidence = confidence;
    msg.success = success;
    msg.neurons_activated = output_size;

    bio_router_broadcast(g_nlang_bio_ctx, &msg, sizeof(msg));
}

/**
 * @brief Handle incoming bio-async messages for neural language
 */
static nimcp_error_t nlang_bio_handler(const void* msg, size_t msg_size,
                                        nimcp_bio_promise_t response_promise,
                                        void* user_data) {
    if (!msg || msg_size < sizeof(bio_message_header_t)) {
        return NIMCP_BIO_ERROR_INVALID_CHANNEL;  // Generic error for invalid input
    }

    const bio_message_header_t* header = (const bio_message_header_t*)msg;

    NIMCP_LOGGING_DEBUG(NLANG_MODULE,
        "Bio-async: received msg type=0x%04X from module=%u",
        header->type, header->source_module);

    switch (header->type) {
        case BIO_MSG_NLP_NEURAL_ENCODE_REQUEST: {
            const bio_msg_nlp_neural_request_t* req =
                (const bio_msg_nlp_neural_request_t*)msg;
            NIMCP_LOGGING_DEBUG(NLANG_MODULE,
                "Neural encode request: ctx=%u size=%u",
                req->context_id, req->data_size);
            // Could trigger encoding pipeline
            break;
        }

        case BIO_MSG_NLP_NEURAL_DECODE_REQUEST: {
            const bio_msg_nlp_neural_request_t* req =
                (const bio_msg_nlp_neural_request_t*)msg;
            NIMCP_LOGGING_DEBUG(NLANG_MODULE,
                "Neural decode request: ctx=%u size=%u",
                req->context_id, req->data_size);
            // Could trigger decoding pipeline
            break;
        }

        case BIO_MSG_ATTENTION_SHIFT: {
            // Attention system directing focus
            const bio_msg_attention_shift_t* shift =
                (const bio_msg_attention_shift_t*)msg;
            NIMCP_LOGGING_DEBUG(NLANG_MODULE,
                "Attention shift: target=%u weight=%.2f",
                shift->target_id, shift->attention_weight);
            break;
        }

        default:
            break;
    }

    (void)response_promise;
    (void)user_data;
    return NIMCP_SUCCESS;
}

//=============================================================================
// KG-Driven Wiring Callback (Phase 2: KG-Based Runtime Module Assembly)
//=============================================================================

/**
 * @brief KG-driven wiring handler callback for neural language module
 *
 * WHAT: Register message handlers based on discovered wiring from KG
 * WHY:  Enables runtime assembly - module discovers its handlers from KG
 * HOW:  Orchestrator invokes this with message types from HANDLES_MESSAGE relations
 *
 * @param ctx Bio-async module context
 * @param message_types Array of message types to handle (from KG)
 * @param message_count Number of message types
 * @param user_data Module context pointer
 * @return 0 on success, -1 on error
 */
static int nlang_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
) {
    if (!ctx || !message_types || message_count == 0) {
        return 0;  /* No handlers to register */
    }

    (void)user_data;

    NIMCP_LOGGING_INFO(NLANG_MODULE,
        "nlang_wiring_handler_callback: registering %u handlers from KG",
        message_count);

    for (uint32_t i = 0; i < message_count; i++) {
        switch (message_types[i]) {
            case BIO_MSG_NLP_NEURAL_ENCODE_REQUEST:
                bio_router_register_handler(ctx, message_types[i], nlang_bio_handler);
                NIMCP_LOGGING_DEBUG(NLANG_MODULE,
                    "  Registered handler for BIO_MSG_NLP_NEURAL_ENCODE_REQUEST");
                break;

            case BIO_MSG_NLP_NEURAL_DECODE_REQUEST:
                bio_router_register_handler(ctx, message_types[i], nlang_bio_handler);
                NIMCP_LOGGING_DEBUG(NLANG_MODULE,
                    "  Registered handler for BIO_MSG_NLP_NEURAL_DECODE_REQUEST");
                break;

            case BIO_MSG_ATTENTION_SHIFT:
                bio_router_register_handler(ctx, message_types[i], nlang_bio_handler);
                NIMCP_LOGGING_DEBUG(NLANG_MODULE,
                    "  Registered handler for BIO_MSG_ATTENTION_SHIFT");
                break;

            default:
                NIMCP_LOGGING_DEBUG(NLANG_MODULE,
                    "  Unknown message type 0x%04X - skipping", message_types[i]);
                break;
        }
    }

    return 0;
}

void nlang_context_init(nlang_shared_context_t* ctx) {
    if (!ctx) return;
    memset(ctx, 0, sizeof(nlang_shared_context_t));
    ctx->version = 1;

    // Register module with BBB and bio-router (once)
    if (!g_nlang_module_registered) {
        bbb_register_module(NLANG_MODULE, BBB_MODULE_TYPE_COGNITIVE);

        // Register with bio-router if initialized
        if (bio_router_is_initialized()) {
            bio_module_info_t bio_info = {
                .module_id = BIO_MODULE_NLP,  // Use NLP module ID
                .module_name = NLANG_MODULE,
                .inbox_capacity = 32,
                .user_data = NULL
            };
            g_nlang_bio_ctx = bio_router_register_module(&bio_info);

            if (g_nlang_bio_ctx) {
                /* KG-Driven Wiring: Register callback for orchestrator to invoke
                 * When orchestrator starts, it discovers HANDLES_MESSAGE relations
                 * from the KG and invokes this callback with the message types */
                nimcp_error_t cb_result = bio_router_register_wiring_callback(
                    BIO_MODULE_NLP,
                    (void*)nlang_wiring_handler_callback,
                    NULL
                );

                if (cb_result != NIMCP_SUCCESS) {
                    /* Fallback: Direct registration if orchestrator not available
                     * This ensures backward compatibility with non-KG systems */
                    LEGACY_HANDLER_REGISTRATION(
                        bio_router_register_handler(g_nlang_bio_ctx,
                            BIO_MSG_NLP_NEURAL_ENCODE_REQUEST, nlang_bio_handler)
                    );
                    LEGACY_HANDLER_REGISTRATION(
                        bio_router_register_handler(g_nlang_bio_ctx,
                            BIO_MSG_NLP_NEURAL_DECODE_REQUEST, nlang_bio_handler)
                    );
                    LEGACY_HANDLER_REGISTRATION(
                        bio_router_register_handler(g_nlang_bio_ctx,
                            BIO_MSG_ATTENTION_SHIFT, nlang_bio_handler)
                    );

                    NIMCP_LOGGING_INFO(NLANG_MODULE,
                        "Registered with bio-router as module 0x%04X (legacy direct registration)",
                        BIO_MODULE_NLP);
                } else {
                    NIMCP_LOGGING_INFO(NLANG_MODULE,
                        "Registered with bio-router as module 0x%04X (KG-driven wiring callback)",
                        BIO_MODULE_NLP);
                }
            }
        }

        g_nlang_module_registered = true;

        bbb_audit_log(BBB_AUDIT_INFO, NLANG_MODULE, "module_init",
                      "Neural language module registered");
    }

    NIMCP_LOGGING_DEBUG("nlang", "Shared context initialized");
}

int nlang_context_define(nlang_shared_context_t* ctx,
                         uint8_t slot,
                         const uint8_t* primitives,
                         size_t count) {
    if (!ctx || !primitives || slot >= NLANG_CONTEXT_SLOTS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nlang_context_init: required parameter is NULL (ctx, primitives)");
        return -1;
    }
    if (count > 16) count = 16;

    nlang_context_slot_t* s = &ctx->slots[slot];
    s->slot_id = slot;
    s->timestamp = 0;  // Would use nimcp_time_now()
    s->primitive_count = (uint8_t)count;
    memcpy(s->primitives, primitives, count);
    s->valid = true;

    ctx->version++;

    NIMCP_LOGGING_DEBUG("nlang", "Context slot %u defined with %zu primitives", slot, count);
    return 0;
}

void nlang_context_set_reference(nlang_shared_context_t* ctx,
                                 double lat_deg,
                                 double lon_deg) {
    if (!ctx) return;

    // Store reference in slot 0's reference_coord
    ctx->slots[0].reference_coord.latitude = (int32_t)(lat_deg * 1e7);
    ctx->slots[0].reference_coord.longitude = (int32_t)(lon_deg * 1e7);

    NIMCP_LOGGING_DEBUG("nlang", "Reference point set: %.6f, %.6f", lat_deg, lon_deg);
}

const nlang_context_slot_t* nlang_context_lookup(const nlang_shared_context_t* ctx,
                                                  uint8_t slot) {
    if (!ctx || slot >= NLANG_CONTEXT_SLOTS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nlang_context_init: ctx is NULL");
        return NULL;
    }

    const nlang_context_slot_t* s = &ctx->slots[slot];
    return s->valid ? s : NULL;
}

void nlang_context_clear(nlang_shared_context_t* ctx, uint8_t slot) {
    if (!ctx || slot >= NLANG_CONTEXT_SLOTS) return;

    ctx->slots[slot].valid = false;
    ctx->version++;

    NIMCP_LOGGING_DEBUG("nlang", "Context slot %u cleared", slot);
}

int nlang_context_serialize(const nlang_shared_context_t* ctx,
                            uint8_t* buffer,
                            size_t buffer_len) {
    if (!ctx || !buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nlang_context_clear: required parameter is NULL (ctx, buffer)");
        return -1;
    }

    // Calculate required size
    size_t required = 4;  // version (4 bytes)
    for (int i = 0; i < NLANG_CONTEXT_SLOTS; i++) {
        if (ctx->slots[i].valid) {
            required += 1 + 1 + ctx->slots[i].primitive_count;  // slot_id + count + primitives
        }
    }
    required += 1;  // terminator

    if (buffer_len < required) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nlang_context_clear: validation failed");
        return -1;
    }

    size_t offset = 0;

    // Version
    buffer[offset++] = (uint8_t)(ctx->version >> 24);
    buffer[offset++] = (uint8_t)(ctx->version >> 16);
    buffer[offset++] = (uint8_t)(ctx->version >> 8);
    buffer[offset++] = (uint8_t)(ctx->version);

    // Valid slots
    for (int i = 0; i < NLANG_CONTEXT_SLOTS; i++) {
        if (ctx->slots[i].valid) {
            buffer[offset++] = (uint8_t)i;
            buffer[offset++] = ctx->slots[i].primitive_count;
            memcpy(&buffer[offset], ctx->slots[i].primitives, ctx->slots[i].primitive_count);
            offset += ctx->slots[i].primitive_count;
        }
    }

    buffer[offset++] = 0xFF;  // Terminator

    return (int)offset;
}

int nlang_context_deserialize(const uint8_t* buffer,
                              size_t buffer_len,
                              nlang_shared_context_t* ctx) {
    if (!buffer || !ctx || buffer_len < 5) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nlang_context_clear: required parameter is NULL (buffer, ctx)");
        return -1;
    }

    nlang_context_init(ctx);

    size_t offset = 0;

    // Version
    ctx->version = ((uint32_t)buffer[offset] << 24) |
                   ((uint32_t)buffer[offset + 1] << 16) |
                   ((uint32_t)buffer[offset + 2] << 8) |
                   buffer[offset + 3];
    offset += 4;

    // Slots
    while (offset < buffer_len && buffer[offset] != 0xFF) {
        uint8_t slot_id = buffer[offset++];
        if (slot_id >= NLANG_CONTEXT_SLOTS) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nlang_context_clear: capacity exceeded");
            return -1;
        }

        if (offset >= buffer_len) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nlang_context_clear: capacity exceeded");
            return -1;
        }
        uint8_t count = buffer[offset++];
        if (count > 16) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nlang_context_clear: validation failed");
            return -1;
        }

        if (offset + count > buffer_len) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nlang_context_clear: validation failed");
            return -1;
        }

        ctx->slots[slot_id].slot_id = slot_id;
        ctx->slots[slot_id].primitive_count = count;
        memcpy(ctx->slots[slot_id].primitives, &buffer[offset], count);
        ctx->slots[slot_id].valid = true;
        offset += count;
    }

    if (offset < buffer_len && buffer[offset] == 0xFF) {
        offset++;
    }

    return (int)offset;
}

//=============================================================================
// Interpreter Implementation
//=============================================================================

const char* nlang_primitive_name(uint8_t primitive) {
    uint8_t cat = (primitive >> 4) & 0x0F;
    uint8_t idx = primitive & 0x0F;

    static char buf[32];

    switch (cat) {
        case 0x0: return percept_names[idx];
        case 0x1: return action_names[idx];
        case 0x2: return spatial_names[idx];
        case 0x3: return temporal_names[idx];
        case 0x4: return quantity_names[idx];
        case 0x5: return social_names[idx];
        case 0x6: return cognitive_names[idx];
        case 0x7: return emotion_names[idx];
        case 0x8: return intent_names[idx];
        case 0x9: return query_names[idx];
        case 0xA: return assert_names[idx];
        case 0xB: return modifier_names[idx];
        case 0xC: return reference_names[idx];
        case 0xD: return domain_names[idx];
        case 0xE: return meta_names[idx];
        case 0xF: return extended_names[idx];
        default:
            snprintf(buf, sizeof(buf), "UNKNOWN_0x%02X", primitive);
            return buf;
    }
}

uint8_t nlang_primitive_category(uint8_t primitive) {
    return (primitive >> 4) & 0x0F;
}

int nlang_interpret(const nlang_expression_t* expr,
                    const nlang_shared_context_t* ctx,
                    nlang_interpretation_t* result) {
    if (!expr || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nlang_primitive_category: required parameter is NULL (expr, result)");
        return -1;
    }

    static char text_buf[NIMCP_ERROR_BUFFER_SIZE];
    size_t text_offset = 0;

    // Build natural language interpretation
    text_offset += snprintf(text_buf + text_offset, sizeof(text_buf) - text_offset,
                           "[%s] ", nlang_primitive_name(expr->intent));

    for (uint8_t i = 0; i < expr->primitive_count && text_offset < sizeof(text_buf) - 20; i++) {
        text_offset += snprintf(text_buf + text_offset, sizeof(text_buf) - text_offset,
                               "%s ", nlang_primitive_name(expr->primitives[i]));
    }

    if (expr->has_coord != NLANG_COORD_NONE) {
        text_offset += snprintf(text_buf + text_offset, sizeof(text_buf) - text_offset,
                               "@LOC ");
    }

    result->natural_language = text_buf;
    result->primary_intent = (nlang_intent_t)expr->intent;
    result->emotion = expr->emotion;
    result->contains_location = (expr->has_coord != NLANG_COORD_NONE);
    result->contains_emotion = (expr->emotion.pleasure != 0 ||
                               expr->emotion.arousal != 0 ||
                               expr->emotion.dominance != 0);

    // Determine urgency from emotion arousal
    int urgency = (expr->emotion.arousal + 128) / 26;  // Map -128..127 to 0..10
    result->urgency_level = (uint8_t)(urgency > 10 ? 10 : urgency);

    // Check if requires response
    result->requires_response = (expr->intent == INTENT_REQUEST ||
                                expr->intent == INTENT_QUERY ||
                                expr->intent == INTENT_NEGOTIATE);

    (void)ctx;  // Context used for reference resolution in full implementation

    return 0;
}

//=============================================================================
// Template Implementation
//=============================================================================

void nlang_template_move_to(nlang_expression_t* expr,
                            int16_t lat_m, int16_t lon_m,
                            nlang_pad_emotion_t urgency) {
    nlang_expr_init(expr, INTENT_COMMAND);
    nlang_expr_add(expr, ACTION_MOVE);
    nlang_expr_add(expr, SPATIAL_THERE);
    nlang_expr_set_coord_short(expr, lat_m, lon_m);
    nlang_expr_set_emotion(expr, urgency);
    nlang_expr_finalize(expr);
}

void nlang_template_threat_report(nlang_expression_t* expr,
                                  int16_t lat_m, int16_t lon_m,
                                  uint8_t threat_type) {
    nlang_expr_init(expr, INTENT_WARN);
    nlang_expr_add(expr, PERCEPT_THREAT);
    nlang_expr_add(expr, threat_type);
    nlang_expr_add(expr, SPATIAL_THERE);
    nlang_expr_set_coord_short(expr, lat_m, lon_m);
    nlang_expr_set_emotion(expr, NLANG_PAD_AFRAID);
    nlang_expr_finalize(expr);
}

void nlang_template_victim_report(nlang_expression_t* expr,
                                  int16_t lat_m, int16_t lon_m,
                                  uint8_t triage, uint8_t count) {
    nlang_expr_init(expr, INTENT_REPORT);
    nlang_expr_add(expr, SOCIAL_VICTIM);
    nlang_expr_add(expr, triage);

    // Add count indication
    if (count == 0) {
        nlang_expr_add(expr, QUANTITY_ZERO);
    } else if (count == 1) {
        nlang_expr_add(expr, QUANTITY_ONE);
    } else if (count <= 5) {
        nlang_expr_add(expr, QUANTITY_FEW);
    } else if (count <= 12) {
        nlang_expr_add(expr, QUANTITY_SEVERAL);
    } else {
        nlang_expr_add(expr, QUANTITY_MANY);
    }

    nlang_expr_add(expr, SPATIAL_THERE);
    nlang_expr_set_coord_short(expr, lat_m, lon_m);

    // Set emotion based on triage level
    if (triage == DOMAIN_TRIAGE_RED || triage == DOMAIN_TRIAGE_BLACK) {
        nlang_expr_set_emotion(expr, NLANG_PAD_URGENT);
    } else {
        nlang_expr_set_emotion(expr, NLANG_PAD_CAUTIOUS);
    }

    nlang_expr_finalize(expr);
}

void nlang_template_status_query(nlang_expression_t* expr,
                                 nlang_query_t query_type) {
    nlang_expr_init(expr, INTENT_QUERY);
    nlang_expr_add(expr, (uint8_t)query_type);
    nlang_expr_set_emotion(expr, NLANG_PAD_NEUTRAL);
    nlang_expr_finalize(expr);
}

void nlang_template_acknowledge(nlang_expression_t* expr,
                                bool success,
                                nlang_pad_emotion_t emotion) {
    nlang_expr_init(expr, INTENT_ACKNOWLEDGE);
    nlang_expr_add(expr, success ? ASSERT_SUCCESS : ASSERT_FAILURE);
    nlang_expr_set_emotion(expr, emotion);
    nlang_expr_finalize(expr);
}

void nlang_template_help_request(nlang_expression_t* expr,
                                 nlang_pad_emotion_t urgency) {
    nlang_expr_init(expr, INTENT_REQUEST);
    nlang_expr_add(expr, SOCIAL_RESCUER);
    nlang_expr_add(expr, SPATIAL_HERE);
    nlang_expr_add(expr, TEMPORAL_IMMEDIATE);
    nlang_expr_set_emotion(expr, urgency);
    nlang_expr_finalize(expr);
}

//=============================================================================
// Bandwidth Statistics Implementation
//=============================================================================

void nlang_calculate_bandwidth_stats(const nlang_expression_t* expr,
                                     nlang_bandwidth_stats_t* stats) {
    if (!expr || !stats) return;

    stats->raw_bytes = nlang_expr_size(expr);
    stats->primitives_used = expr->primitive_count;

    // Estimate equivalent text size
    // Average: "COMMAND MOVE TO location(30 chars) urgently" ≈ 50 bytes
    size_t text_estimate = 10;  // Intent overhead
    text_estimate += expr->primitive_count * 8;  // ~8 chars per primitive average
    if (expr->has_coord) text_estimate += 30;  // Location text
    if (expr->emotion.pleasure != 0 || expr->emotion.arousal != 0) {
        text_estimate += 15;  // Emotion descriptor
    }

    stats->equivalent_text_bytes = text_estimate;
    stats->compression_ratio = (float)text_estimate / (float)stats->raw_bytes;
}
