//=============================================================================
// nimcp_neural_language.h - Neural Language Semantic Primitives
//=============================================================================
/**
 * @file nimcp_neural_language.h
 * @brief Bandwidth-efficient semantic language for brain-to-brain communication
 *
 * WHAT: 256 cognitive primitives + PAD emotions + semantic expressions
 * WHY:  Enable meaningful communication under bandwidth/jamming constraints
 * HOW:  1-byte primitives, 3-byte emotions, 4-byte coordinates, semantic checksum
 *
 * BANDWIDTH EFFICIENCY:
 * - Traditional: "Move to coordinates 45.2,-122.5 urgently with high priority"
 *   = ~60 bytes plaintext + encryption overhead
 * - Neural Language: INTENT_COMMAND + ACTION_MOVE + location(4) + URGENCY_HIGH
 *   = 7 bytes total
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#ifndef NIMCP_NEURAL_LANGUAGE_H
#define NIMCP_NEURAL_LANGUAGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Cognitive Primitive Categories (16 categories x 16 primitives = 256 total)
//=============================================================================

/**
 * @brief Category index occupies high nibble (0x00-0xF0)
 * @brief Primitive index occupies low nibble (0x00-0x0F)
 */

//-----------------------------------------------------------------------------
// Category 0x00: Perception Primitives
//-----------------------------------------------------------------------------
typedef enum {
    PERCEPT_NULL         = 0x00,  // No perception / end marker
    PERCEPT_SEE          = 0x01,  // Visual detection
    PERCEPT_HEAR         = 0x02,  // Auditory detection
    PERCEPT_TOUCH        = 0x03,  // Tactile contact
    PERCEPT_SMELL        = 0x04,  // Olfactory detection
    PERCEPT_TASTE        = 0x05,  // Gustatory detection
    PERCEPT_TEMPERATURE  = 0x06,  // Thermal sensing
    PERCEPT_PRESSURE     = 0x07,  // Barometric/contact pressure
    PERCEPT_MOTION       = 0x08,  // Movement detection
    PERCEPT_PROXIMITY    = 0x09,  // Near object detection
    PERCEPT_OBSTACLE     = 0x0A,  // Blocking object
    PERCEPT_TARGET       = 0x0B,  // Target acquisition
    PERCEPT_THREAT       = 0x0C,  // Danger detection
    PERCEPT_FRIENDLY     = 0x0D,  // Ally identification
    PERCEPT_UNKNOWN      = 0x0E,  // Unidentified entity
    PERCEPT_SIGNAL       = 0x0F,  // Communication signal
} nlang_percept_t;

//-----------------------------------------------------------------------------
// Category 0x10: Action Primitives
//-----------------------------------------------------------------------------
typedef enum {
    ACTION_STOP          = 0x10,  // Cease activity
    ACTION_MOVE          = 0x11,  // Locomotion
    ACTION_TURN          = 0x12,  // Change heading
    ACTION_ASCEND        = 0x13,  // Gain altitude
    ACTION_DESCEND       = 0x14,  // Lose altitude
    ACTION_HOVER         = 0x15,  // Maintain position
    ACTION_FOLLOW        = 0x16,  // Track entity
    ACTION_EVADE         = 0x17,  // Avoid entity
    ACTION_APPROACH      = 0x18,  // Move toward
    ACTION_RETREAT       = 0x19,  // Move away
    ACTION_ORBIT         = 0x1A,  // Circle position
    ACTION_SCAN          = 0x1B,  // Search area
    ACTION_ACQUIRE       = 0x1C,  // Lock onto target
    ACTION_RELEASE       = 0x1D,  // Drop/release item
    ACTION_DEPLOY        = 0x1E,  // Activate payload
    ACTION_RETURN        = 0x1F,  // Return to base
} nlang_action_t;

//-----------------------------------------------------------------------------
// Category 0x20: Spatial Primitives
//-----------------------------------------------------------------------------
typedef enum {
    SPATIAL_HERE         = 0x20,  // Current location
    SPATIAL_THERE        = 0x21,  // Referenced location
    SPATIAL_NEAR         = 0x22,  // Close proximity
    SPATIAL_FAR          = 0x23,  // Distant
    SPATIAL_ABOVE        = 0x24,  // Higher position
    SPATIAL_BELOW        = 0x25,  // Lower position
    SPATIAL_LEFT         = 0x26,  // Port side
    SPATIAL_RIGHT        = 0x27,  // Starboard side
    SPATIAL_FRONT        = 0x28,  // Forward direction
    SPATIAL_BEHIND       = 0x29,  // Rear direction
    SPATIAL_INSIDE       = 0x2A,  // Within boundary
    SPATIAL_OUTSIDE      = 0x2B,  // Beyond boundary
    SPATIAL_CENTER       = 0x2C,  // Midpoint
    SPATIAL_EDGE         = 0x2D,  // Boundary
    SPATIAL_PATH         = 0x2E,  // Route/trajectory
    SPATIAL_ZONE         = 0x2F,  // Defined area
} nlang_spatial_t;

//-----------------------------------------------------------------------------
// Category 0x30: Temporal Primitives
//-----------------------------------------------------------------------------
typedef enum {
    TEMPORAL_NOW         = 0x30,  // Current moment
    TEMPORAL_THEN        = 0x31,  // Referenced time
    TEMPORAL_BEFORE      = 0x32,  // Prior to
    TEMPORAL_AFTER       = 0x33,  // Following
    TEMPORAL_DURING      = 0x34,  // Concurrent with
    TEMPORAL_UNTIL       = 0x35,  // Up to point
    TEMPORAL_SINCE       = 0x36,  // From point
    TEMPORAL_ALWAYS      = 0x37,  // Continuous
    TEMPORAL_NEVER       = 0x38,  // No occurrence
    TEMPORAL_SOON        = 0x39,  // Imminent
    TEMPORAL_LATER       = 0x3A,  // Delayed
    TEMPORAL_IMMEDIATE   = 0x3B,  // Without delay
    TEMPORAL_INTERVAL    = 0x3C,  // Time period
    TEMPORAL_CYCLE       = 0x3D,  // Repeating pattern
    TEMPORAL_DEADLINE    = 0x3E,  // Time limit
    TEMPORAL_DURATION    = 0x3F,  // Length of time
} nlang_temporal_t;

//-----------------------------------------------------------------------------
// Category 0x40: Quantity Primitives
//-----------------------------------------------------------------------------
typedef enum {
    QUANTITY_ZERO        = 0x40,  // None
    QUANTITY_ONE         = 0x41,  // Single
    QUANTITY_FEW         = 0x42,  // Small number (2-5)
    QUANTITY_SEVERAL     = 0x43,  // Medium number (6-12)
    QUANTITY_MANY        = 0x44,  // Large number (>12)
    QUANTITY_ALL         = 0x45,  // Complete set
    QUANTITY_SOME        = 0x46,  // Partial set
    QUANTITY_NONE        = 0x47,  // Empty set
    QUANTITY_MORE        = 0x48,  // Increase
    QUANTITY_LESS        = 0x49,  // Decrease
    QUANTITY_HALF        = 0x4A,  // 50%
    QUANTITY_MOST        = 0x4B,  // Majority
    QUANTITY_MINIMUM     = 0x4C,  // Lowest value
    QUANTITY_MAXIMUM     = 0x4D,  // Highest value
    QUANTITY_AVERAGE     = 0x4E,  // Mean value
    QUANTITY_EXACT       = 0x4F,  // Precise count follows
} nlang_quantity_t;

//-----------------------------------------------------------------------------
// Category 0x50: Social/Swarm Primitives
//-----------------------------------------------------------------------------
typedef enum {
    SOCIAL_SELF          = 0x50,  // This agent
    SOCIAL_OTHER         = 0x51,  // Another agent
    SOCIAL_GROUP         = 0x52,  // Agent collective
    SOCIAL_LEADER        = 0x53,  // Command authority
    SOCIAL_FOLLOWER      = 0x54,  // Subordinate agent
    SOCIAL_ALLY          = 0x55,  // Friendly entity
    SOCIAL_ENEMY         = 0x56,  // Hostile entity
    SOCIAL_NEUTRAL       = 0x57,  // Non-aligned entity
    SOCIAL_CIVILIAN      = 0x58,  // Non-combatant
    SOCIAL_VICTIM        = 0x59,  // Person needing help
    SOCIAL_RESCUER       = 0x5A,  // Help provider
    SOCIAL_TEAM_ALPHA    = 0x5B,  // Team designation A
    SOCIAL_TEAM_BETA     = 0x5C,  // Team designation B
    SOCIAL_TEAM_GAMMA    = 0x5D,  // Team designation C
    SOCIAL_BROADCAST     = 0x5E,  // All agents
    SOCIAL_MASTER        = 0x5F,  // Master brain
} nlang_social_t;

//-----------------------------------------------------------------------------
// Category 0x60: Cognitive State Primitives
//-----------------------------------------------------------------------------
typedef enum {
    COGNITIVE_KNOW       = 0x60,  // Certainty
    COGNITIVE_BELIEVE    = 0x61,  // Probable
    COGNITIVE_DOUBT      = 0x62,  // Uncertain
    COGNITIVE_UNKNOWN    = 0x63,  // No data
    COGNITIVE_EXPECT     = 0x64,  // Anticipate
    COGNITIVE_REMEMBER   = 0x65,  // Recall
    COGNITIVE_FORGET     = 0x66,  // Memory loss
    COGNITIVE_LEARN      = 0x67,  // Acquire knowledge
    COGNITIVE_RECOGNIZE  = 0x68,  // Pattern match
    COGNITIVE_DECIDE     = 0x69,  // Make choice
    COGNITIVE_PLAN       = 0x6A,  // Future action
    COGNITIVE_PREDICT    = 0x6B,  // Forecast
    COGNITIVE_CONFUSED   = 0x6C,  // Processing error
    COGNITIVE_ALERT      = 0x6D,  // High attention
    COGNITIVE_FOCUS      = 0x6E,  // Concentrated
    COGNITIVE_DISTRACTED = 0x6F,  // Attention diverted
} nlang_cognitive_t;

//-----------------------------------------------------------------------------
// Category 0x70: Emotion Primitives (basic emotions, detailed via PAD)
//-----------------------------------------------------------------------------
typedef enum {
    EMOTION_NEUTRAL      = 0x70,  // Baseline
    EMOTION_HAPPY        = 0x71,  // Positive valence
    EMOTION_SAD          = 0x72,  // Negative valence
    EMOTION_ANGRY        = 0x73,  // Hostile
    EMOTION_AFRAID       = 0x74,  // Fearful
    EMOTION_SURPRISED    = 0x75,  // Unexpected
    EMOTION_DISGUSTED    = 0x76,  // Aversion
    EMOTION_CURIOUS      = 0x77,  // Interest
    EMOTION_FRUSTRATED   = 0x78,  // Blocked goal
    EMOTION_CONFIDENT    = 0x79,  // Self-assured
    EMOTION_CAUTIOUS     = 0x7A,  // Wary
    EMOTION_URGENT       = 0x7B,  // Time pressure
    EMOTION_CALM         = 0x7C,  // Low arousal
    EMOTION_STRESSED     = 0x7D,  // High load
    EMOTION_HOPEFUL      = 0x7E,  // Positive expectation
    EMOTION_DESPERATE    = 0x7F,  // Critical situation
} nlang_emotion_t;

//-----------------------------------------------------------------------------
// Category 0x80: Intent Primitives
//-----------------------------------------------------------------------------
typedef enum {
    INTENT_INFORM        = 0x80,  // Share information
    INTENT_REQUEST       = 0x81,  // Ask for action
    INTENT_COMMAND       = 0x82,  // Order action
    INTENT_SUGGEST       = 0x83,  // Propose action
    INTENT_WARN          = 0x84,  // Alert to danger
    INTENT_PROMISE       = 0x85,  // Commit to action
    INTENT_REFUSE        = 0x86,  // Decline request
    INTENT_ACCEPT        = 0x87,  // Agree to request
    INTENT_ACKNOWLEDGE   = 0x88,  // Confirm receipt
    INTENT_QUERY         = 0x89,  // Request information
    INTENT_REPORT        = 0x8A,  // Status update
    INTENT_NEGOTIATE     = 0x8B,  // Bargain
    INTENT_COORDINATE    = 0x8C,  // Synchronize action
    INTENT_DELEGATE      = 0x8D,  // Assign task
    INTENT_ESCALATE      = 0x8E,  // Increase priority
    INTENT_CANCEL        = 0x8F,  // Abort task
} nlang_intent_t;

//-----------------------------------------------------------------------------
// Category 0x90: Query Primitives
//-----------------------------------------------------------------------------
typedef enum {
    QUERY_WHAT           = 0x90,  // Identity query
    QUERY_WHERE          = 0x91,  // Location query
    QUERY_WHEN           = 0x92,  // Time query
    QUERY_WHO            = 0x93,  // Agent query
    QUERY_WHY            = 0x94,  // Reason query
    QUERY_HOW            = 0x95,  // Method query
    QUERY_HOWMANY        = 0x96,  // Quantity query
    QUERY_WHICH          = 0x97,  // Selection query
    QUERY_STATUS         = 0x98,  // State query
    QUERY_READY          = 0x99,  // Readiness query
    QUERY_CAPABLE        = 0x9A,  // Ability query
    QUERY_SAFE           = 0x9B,  // Safety query
    QUERY_PRIORITY       = 0x9C,  // Urgency query
    QUERY_CONFIRM        = 0x9D,  // Verification query
    QUERY_REPEAT         = 0x9E,  // Retransmit request
    QUERY_ELABORATE      = 0x9F,  // More detail request
} nlang_query_t;

//-----------------------------------------------------------------------------
// Category 0xA0: Assertion Primitives
//-----------------------------------------------------------------------------
typedef enum {
    ASSERT_YES           = 0xA0,  // Affirmative
    ASSERT_NO            = 0xA1,  // Negative
    ASSERT_MAYBE         = 0xA2,  // Uncertain
    ASSERT_TRUE          = 0xA3,  // Verified fact
    ASSERT_FALSE         = 0xA4,  // Disproven
    ASSERT_POSSIBLE      = 0xA5,  // Feasible
    ASSERT_IMPOSSIBLE    = 0xA6,  // Not feasible
    ASSERT_LIKELY        = 0xA7,  // High probability
    ASSERT_UNLIKELY      = 0xA8,  // Low probability
    ASSERT_CONFIRMED     = 0xA9,  // Validated
    ASSERT_UNCONFIRMED   = 0xAA,  // Not validated
    ASSERT_SUCCESS       = 0xAB,  // Goal achieved
    ASSERT_FAILURE       = 0xAC,  // Goal not achieved
    ASSERT_PARTIAL       = 0xAD,  // Incomplete
    ASSERT_COMPLETE      = 0xAE,  // Finished
    ASSERT_ONGOING       = 0xAF,  // In progress
} nlang_assert_t;

//-----------------------------------------------------------------------------
// Category 0xB0: Modifier Primitives
//-----------------------------------------------------------------------------
typedef enum {
    MOD_VERY             = 0xB0,  // Intensifier
    MOD_SLIGHTLY         = 0xB1,  // Diminisher
    MOD_NOT              = 0xB2,  // Negation
    MOD_ALSO             = 0xB3,  // Addition
    MOD_ONLY             = 0xB4,  // Exclusion
    MOD_MAYBE            = 0xB5,  // Uncertainty
    MOD_DEFINITELY       = 0xB6,  // Certainty
    MOD_PROBABLY         = 0xB7,  // Likelihood
    MOD_QUICKLY          = 0xB8,  // Speed increase
    MOD_SLOWLY           = 0xB9,  // Speed decrease
    MOD_CAREFULLY        = 0xBA,  // Caution
    MOD_QUIETLY          = 0xBB,  // Stealth
    MOD_LOUDLY           = 0xBC,  // Broadcast
    MOD_REPEATEDLY       = 0xBD,  // Iteration
    MOD_ALTERNATIVELY    = 0xBE,  // Option
    MOD_CONDITIONALLY    = 0xBF,  // If-then
} nlang_modifier_t;

//-----------------------------------------------------------------------------
// Category 0xC0: Reference Primitives
//-----------------------------------------------------------------------------
typedef enum {
    REF_THIS             = 0xC0,  // Current context
    REF_THAT             = 0xC1,  // Previous context
    REF_PREVIOUS         = 0xC2,  // Last message
    REF_NEXT             = 0xC3,  // Following message
    REF_FIRST            = 0xC4,  // Initial item
    REF_LAST             = 0xC5,  // Final item
    REF_SAME             = 0xC6,  // Identical
    REF_DIFFERENT        = 0xC7,  // Distinct
    REF_CONTEXT_1        = 0xC8,  // Shared context slot 1
    REF_CONTEXT_2        = 0xC9,  // Shared context slot 2
    REF_CONTEXT_3        = 0xCA,  // Shared context slot 3
    REF_CONTEXT_4        = 0xCB,  // Shared context slot 4
    REF_TARGET_A         = 0xCC,  // Target reference A
    REF_TARGET_B         = 0xCD,  // Target reference B
    REF_WAYPOINT         = 0xCE,  // Location reference
    REF_MISSION          = 0xCF,  // Task reference
} nlang_reference_t;

//-----------------------------------------------------------------------------
// Category 0xD0: Domain-Specific Primitives (SAR/Disaster)
//-----------------------------------------------------------------------------
typedef enum {
    DOMAIN_FIRE          = 0xD0,  // Fire hazard
    DOMAIN_FLOOD         = 0xD1,  // Water hazard
    DOMAIN_COLLAPSE      = 0xD2,  // Structural failure
    DOMAIN_CHEMICAL      = 0xD3,  // Chemical hazard
    DOMAIN_RADIATION     = 0xD4,  // Radiation hazard
    DOMAIN_MEDICAL       = 0xD5,  // Medical emergency
    DOMAIN_TRIAGE_GREEN  = 0xD6,  // Minor injury
    DOMAIN_TRIAGE_YELLOW = 0xD7,  // Delayed treatment
    DOMAIN_TRIAGE_RED    = 0xD8,  // Immediate treatment
    DOMAIN_TRIAGE_BLACK  = 0xD9,  // Deceased
    DOMAIN_EVACUATION    = 0xDA,  // Evacuation needed
    DOMAIN_SHELTER       = 0xDB,  // Safe location
    DOMAIN_SUPPLIES      = 0xDC,  // Resources
    DOMAIN_ROUTE_CLEAR   = 0xDD,  // Passable path
    DOMAIN_ROUTE_BLOCKED = 0xDE,  // Impassable path
    DOMAIN_LZ            = 0xDF,  // Landing zone
} nlang_domain_t;

//-----------------------------------------------------------------------------
// Category 0xE0: Meta/Protocol Primitives
//-----------------------------------------------------------------------------
typedef enum {
    META_BEGIN           = 0xE0,  // Start expression
    META_END             = 0xE1,  // End expression
    META_SEQUENCE        = 0xE2,  // Ordered list follows
    META_PARALLEL        = 0xE3,  // Unordered list follows
    META_CONDITION       = 0xE4,  // If-condition follows
    META_CONSEQUENCE     = 0xE5,  // Then-action follows
    META_ALTERNATIVE     = 0xE6,  // Else-action follows
    META_LOOP            = 0xE7,  // Repeat marker
    META_REFERENCE       = 0xE8,  // Context lookup follows
    META_DEFINE          = 0xE9,  // Context store follows
    META_EXTEND          = 0xEA,  // Extended primitive follows
    META_LITERAL         = 0xEB,  // Raw bytes follow
    META_COMPRESS        = 0xEC,  // Compressed data follows
    META_CHECKSUM        = 0xED,  // Semantic checksum follows
    META_VERSION         = 0xEE,  // Protocol version
    META_RESERVED        = 0xEF,  // Reserved for future
} nlang_meta_t;

//-----------------------------------------------------------------------------
// Category 0xF0: Extended Primitives (256+ via META_EXTEND prefix)
//-----------------------------------------------------------------------------
typedef enum {
    EXT_WEATHER_CLEAR    = 0xF0,  // Clear weather
    EXT_WEATHER_RAIN     = 0xF1,  // Precipitation
    EXT_WEATHER_WIND     = 0xF2,  // High wind
    EXT_WEATHER_FOG      = 0xF3,  // Low visibility
    EXT_BATTERY_FULL     = 0xF4,  // Full charge
    EXT_BATTERY_LOW      = 0xF5,  // Low charge
    EXT_BATTERY_CRITICAL = 0xF6,  // Critical charge
    EXT_SENSOR_OFFLINE   = 0xF7,  // Sensor failure
    EXT_COMMS_DEGRADED   = 0xF8,  // Partial connectivity
    EXT_COMMS_LOST       = 0xF9,  // No connectivity
    EXT_GPS_LOCK         = 0xFA,  // Position confirmed
    EXT_GPS_LOST         = 0xFB,  // No position fix
    EXT_PAYLOAD_READY    = 0xFC,  // Payload operational
    EXT_PAYLOAD_DEPLOYED = 0xFD,  // Payload released
    EXT_PAYLOAD_EMPTY    = 0xFE,  // No payload
    EXT_ESCAPE           = 0xFF,  // Extended escape code
} nlang_extended_t;

//=============================================================================
// PAD Emotional Model (Pleasure-Arousal-Dominance)
//=============================================================================

/**
 * @brief Compact emotional state representation
 *
 * Each dimension is encoded as int8_t:
 * - Pleasure:  -128 (miserable) to +127 (ecstatic)
 * - Arousal:   -128 (sleepy) to +127 (frenzied)
 * - Dominance: -128 (submissive) to +127 (dominant)
 *
 * Total: 3 bytes for full emotional state
 *
 * Common emotional states:
 * - Happy:     P=+100, A=+50,  D=+50  (positive, active, in control)
 * - Sad:       P=-80,  A=-30, D=-40  (negative, passive, helpless)
 * - Angry:     P=-60,  A=+100, D=+80  (negative, very active, dominant)
 * - Afraid:    P=-70,  A=+80,  D=-70  (negative, active, submissive)
 * - Calm:      P=+20,  A=-50,  D=+20  (neutral, passive, stable)
 * - Urgent:    P=-20,  A=+100, D=+40  (slightly neg, very active, assertive)
 */
typedef struct __attribute__((packed)) {
    int8_t pleasure;    // Valence: negative to positive
    int8_t arousal;     // Activation: calm to excited
    int8_t dominance;   // Control: submissive to dominant
} nlang_pad_emotion_t;

// Pre-defined PAD states for common emotions
#define NLANG_PAD_NEUTRAL     ((nlang_pad_emotion_t){0, 0, 0})
#define NLANG_PAD_HAPPY       ((nlang_pad_emotion_t){100, 50, 50})
#define NLANG_PAD_SAD         ((nlang_pad_emotion_t){-80, -30, -40})
#define NLANG_PAD_ANGRY       ((nlang_pad_emotion_t){-60, 100, 80})
#define NLANG_PAD_AFRAID      ((nlang_pad_emotion_t){-70, 80, -70})
#define NLANG_PAD_CURIOUS     ((nlang_pad_emotion_t){40, 60, 20})
#define NLANG_PAD_CONFIDENT   ((nlang_pad_emotion_t){60, 30, 90})
#define NLANG_PAD_CAUTIOUS    ((nlang_pad_emotion_t){-10, 40, -20})
#define NLANG_PAD_URGENT      ((nlang_pad_emotion_t){-20, 100, 40})
#define NLANG_PAD_CALM        ((nlang_pad_emotion_t){20, -50, 20})
#define NLANG_PAD_STRESSED    ((nlang_pad_emotion_t){-40, 80, -30})
#define NLANG_PAD_DESPERATE   ((nlang_pad_emotion_t){-90, 100, -60})

//=============================================================================
// Compact Coordinate Encoding
//=============================================================================

/**
 * @brief Short coordinate encoding (4 bytes)
 *
 * For operations within ~32km radius:
 * - 16-bit relative latitude  (0.5m resolution)
 * - 16-bit relative longitude (0.5m resolution)
 *
 * Requires shared reference point (set via context)
 */
typedef struct __attribute__((packed)) {
    int16_t lat_offset;    // Meters from reference latitude
    int16_t lon_offset;    // Meters from reference longitude
} nlang_short_coord_t;

/**
 * @brief Full coordinate encoding (8 bytes)
 *
 * For initial reference or long-range:
 * - 32-bit latitude  (1e-7 degree resolution ≈ 1cm)
 * - 32-bit longitude (1e-7 degree resolution ≈ 1cm)
 */
typedef struct __attribute__((packed)) {
    int32_t latitude;      // Degrees * 1e7
    int32_t longitude;     // Degrees * 1e7
} nlang_full_coord_t;

/**
 * @brief 3D coordinate with altitude (6 bytes)
 */
typedef struct __attribute__((packed)) {
    int16_t lat_offset;    // Meters from reference
    int16_t lon_offset;    // Meters from reference
    int16_t altitude;      // Meters AGL (signed for underground)
} nlang_coord_3d_t;

//=============================================================================
// Expression Structure
//=============================================================================

/**
 * @brief Maximum expression length (primitives)
 */
#define NLANG_MAX_EXPRESSION_LEN 32

/**
 * @brief Maximum shared context slots
 */
#define NLANG_CONTEXT_SLOTS 8

/**
 * @brief Semantic expression structure
 *
 * Expressions encode complete thoughts:
 * - Intent: What kind of communication (inform, command, query)
 * - Primitives: Sequence of cognitive primitives
 * - Emotion: PAD emotional state
 * - Coordinates: Optional location data
 * - Context refs: References to shared context
 */
typedef struct __attribute__((packed)) {
    uint8_t version;                           // Protocol version
    uint8_t intent;                            // Primary intent (nlang_intent_t)
    uint8_t primitive_count;                   // Number of primitives
    uint8_t primitives[NLANG_MAX_EXPRESSION_LEN]; // Primitive sequence
    nlang_pad_emotion_t emotion;               // Emotional state
    uint8_t has_coord;                         // Coordinate flag (0/1/2/3)
    union {
        nlang_short_coord_t short_coord;       // 2D relative (4 bytes)
        nlang_coord_3d_t coord_3d;             // 3D relative (6 bytes)
        nlang_full_coord_t full_coord;         // Full GPS (8 bytes)
    } coord;
    uint8_t context_refs;                      // Bitmap of referenced context slots
    uint16_t semantic_checksum;                // Semantic integrity check
} nlang_expression_t;

/**
 * @brief Coordinate encoding type
 */
typedef enum {
    NLANG_COORD_NONE   = 0,   // No coordinates
    NLANG_COORD_SHORT  = 1,   // 2D relative (4 bytes)
    NLANG_COORD_3D     = 2,   // 3D relative (6 bytes)
    NLANG_COORD_FULL   = 3,   // Full GPS (8 bytes)
} nlang_coord_type_t;

//=============================================================================
// Shared Context
//=============================================================================

/**
 * @brief Shared context entry
 *
 * Allows compression via reference:
 * - Define a complex expression once
 * - Reference it with single REF_CONTEXT_N primitive
 */
typedef struct {
    uint32_t slot_id;                  // Slot identifier (0-7)
    uint64_t timestamp;                // When defined
    uint8_t primitive_count;           // Number of primitives
    uint8_t primitives[16];            // Primitive sequence
    nlang_full_coord_t reference_coord; // Reference for short coords
    bool valid;                        // Slot in use
} nlang_context_slot_t;

/**
 * @brief Shared context state
 */
typedef struct {
    nlang_context_slot_t slots[NLANG_CONTEXT_SLOTS];
    uint64_t last_sync;                // Last context synchronization
    uint32_t version;                  // Context version for sync
} nlang_shared_context_t;

//=============================================================================
// Expression Builder API
//=============================================================================

/**
 * @brief Initialize expression builder
 * @param expr Expression to initialize
 * @param intent Primary intent
 */
void nlang_expr_init(nlang_expression_t* expr, nlang_intent_t intent);

/**
 * @brief Add primitive to expression
 * @param expr Expression to modify
 * @param primitive Primitive to add
 * @return 0 on success, -1 if full
 */
int nlang_expr_add(nlang_expression_t* expr, uint8_t primitive);

/**
 * @brief Add multiple primitives
 * @param expr Expression to modify
 * @param primitives Array of primitives
 * @param count Number of primitives
 * @return 0 on success, -1 if insufficient space
 */
int nlang_expr_add_sequence(nlang_expression_t* expr,
                            const uint8_t* primitives,
                            size_t count);

/**
 * @brief Set emotional state
 * @param expr Expression to modify
 * @param emotion PAD emotional state
 */
void nlang_expr_set_emotion(nlang_expression_t* expr, nlang_pad_emotion_t emotion);

/**
 * @brief Set short coordinates (relative to context reference)
 * @param expr Expression to modify
 * @param lat_meters Latitude offset in meters
 * @param lon_meters Longitude offset in meters
 */
void nlang_expr_set_coord_short(nlang_expression_t* expr,
                                int16_t lat_meters,
                                int16_t lon_meters);

/**
 * @brief Set 3D coordinates
 * @param expr Expression to modify
 * @param lat_meters Latitude offset in meters
 * @param lon_meters Longitude offset in meters
 * @param alt_meters Altitude in meters AGL
 */
void nlang_expr_set_coord_3d(nlang_expression_t* expr,
                             int16_t lat_meters,
                             int16_t lon_meters,
                             int16_t alt_meters);

/**
 * @brief Set full GPS coordinates
 * @param expr Expression to modify
 * @param lat_deg Latitude in degrees
 * @param lon_deg Longitude in degrees
 */
void nlang_expr_set_coord_full(nlang_expression_t* expr,
                               double lat_deg,
                               double lon_deg);

/**
 * @brief Add context reference
 * @param expr Expression to modify
 * @param slot Context slot (0-7)
 */
void nlang_expr_add_context_ref(nlang_expression_t* expr, uint8_t slot);

/**
 * @brief Finalize expression (compute checksum)
 * @param expr Expression to finalize
 */
void nlang_expr_finalize(nlang_expression_t* expr);

//=============================================================================
// Serialization API
//=============================================================================

/**
 * @brief Serialize expression to bytes
 * @param expr Expression to serialize
 * @param buffer Output buffer
 * @param buffer_len Buffer size
 * @return Bytes written, or -1 on error
 */
int nlang_expr_serialize(const nlang_expression_t* expr,
                         uint8_t* buffer,
                         size_t buffer_len);

/**
 * @brief Deserialize expression from bytes
 * @param buffer Input buffer
 * @param buffer_len Buffer size
 * @param expr Output expression
 * @return Bytes consumed, or -1 on error
 */
int nlang_expr_deserialize(const uint8_t* buffer,
                           size_t buffer_len,
                           nlang_expression_t* expr);

/**
 * @brief Get serialized expression size
 * @param expr Expression to measure
 * @return Size in bytes
 */
size_t nlang_expr_size(const nlang_expression_t* expr);

//=============================================================================
// Validation API
//=============================================================================

/**
 * @brief Validate expression integrity
 * @param expr Expression to validate
 * @return true if valid
 */
bool nlang_expr_validate(const nlang_expression_t* expr);

/**
 * @brief Verify semantic checksum
 * @param expr Expression to verify
 * @return true if checksum matches
 */
bool nlang_expr_verify_checksum(const nlang_expression_t* expr);

/**
 * @brief Compute semantic checksum
 * @param expr Expression to checksum
 * @return 16-bit checksum
 *
 * Semantic checksum validates meaning, not just bytes:
 * - Different orderings of equivalent primitives = same checksum
 * - Includes intent, emotion, and coordinate presence
 */
uint16_t nlang_compute_semantic_checksum(const nlang_expression_t* expr);

//=============================================================================
// Context Management API
//=============================================================================

/**
 * @brief Initialize shared context
 * @param ctx Context to initialize
 */
void nlang_context_init(nlang_shared_context_t* ctx);

/**
 * @brief Define context slot
 * @param ctx Shared context
 * @param slot Slot number (0-7)
 * @param primitives Primitive sequence
 * @param count Number of primitives
 * @return 0 on success
 */
int nlang_context_define(nlang_shared_context_t* ctx,
                         uint8_t slot,
                         const uint8_t* primitives,
                         size_t count);

/**
 * @brief Set reference coordinate for short coord encoding
 * @param ctx Shared context
 * @param lat_deg Reference latitude
 * @param lon_deg Reference longitude
 */
void nlang_context_set_reference(nlang_shared_context_t* ctx,
                                 double lat_deg,
                                 double lon_deg);

/**
 * @brief Lookup context slot
 * @param ctx Shared context
 * @param slot Slot number
 * @return Context slot or NULL
 */
const nlang_context_slot_t* nlang_context_lookup(const nlang_shared_context_t* ctx,
                                                  uint8_t slot);

/**
 * @brief Clear context slot
 * @param ctx Shared context
 * @param slot Slot number
 */
void nlang_context_clear(nlang_shared_context_t* ctx, uint8_t slot);

/**
 * @brief Serialize context for synchronization
 * @param ctx Context to serialize
 * @param buffer Output buffer
 * @param buffer_len Buffer size
 * @return Bytes written
 */
int nlang_context_serialize(const nlang_shared_context_t* ctx,
                            uint8_t* buffer,
                            size_t buffer_len);

/**
 * @brief Deserialize context from sync message
 * @param buffer Input buffer
 * @param buffer_len Buffer size
 * @param ctx Output context
 * @return Bytes consumed
 */
int nlang_context_deserialize(const uint8_t* buffer,
                              size_t buffer_len,
                              nlang_shared_context_t* ctx);

//=============================================================================
// Interpreter API
//=============================================================================

/**
 * @brief Expression interpretation result
 */
typedef struct {
    const char* natural_language;     // Human-readable interpretation
    uint8_t urgency_level;            // 0-10 urgency
    bool requires_response;           // Expects reply
    bool contains_location;           // Has coordinates
    bool contains_emotion;            // Has emotional content
    nlang_intent_t primary_intent;    // Main intent
    nlang_pad_emotion_t emotion;      // Emotional state
} nlang_interpretation_t;

/**
 * @brief Interpret expression to human-readable form
 * @param expr Expression to interpret
 * @param ctx Shared context (for reference resolution)
 * @param result Output interpretation
 * @return 0 on success
 */
int nlang_interpret(const nlang_expression_t* expr,
                    const nlang_shared_context_t* ctx,
                    nlang_interpretation_t* result);

/**
 * @brief Get primitive name
 * @param primitive Primitive value
 * @return Static string name
 */
const char* nlang_primitive_name(uint8_t primitive);

/**
 * @brief Get primitive category
 * @param primitive Primitive value
 * @return Category index (0-15)
 */
uint8_t nlang_primitive_category(uint8_t primitive);

//=============================================================================
// Common Expression Templates
//=============================================================================

/**
 * @brief Build "move to location" command
 * @param expr Output expression
 * @param lat_m Latitude offset (meters)
 * @param lon_m Longitude offset (meters)
 * @param urgency Urgency emotion
 */
void nlang_template_move_to(nlang_expression_t* expr,
                            int16_t lat_m, int16_t lon_m,
                            nlang_pad_emotion_t urgency);

/**
 * @brief Build "threat detected" report
 * @param expr Output expression
 * @param lat_m Latitude offset (meters)
 * @param lon_m Longitude offset (meters)
 * @param threat_type Threat primitive (PERCEPT_THREAT, etc.)
 */
void nlang_template_threat_report(nlang_expression_t* expr,
                                  int16_t lat_m, int16_t lon_m,
                                  uint8_t threat_type);

/**
 * @brief Build "victim found" SAR report
 * @param expr Output expression
 * @param lat_m Latitude offset (meters)
 * @param lon_m Longitude offset (meters)
 * @param triage Triage level (DOMAIN_TRIAGE_*)
 * @param count Victim count
 */
void nlang_template_victim_report(nlang_expression_t* expr,
                                  int16_t lat_m, int16_t lon_m,
                                  uint8_t triage, uint8_t count);

/**
 * @brief Build status query
 * @param expr Output expression
 * @param query_type Query primitive (QUERY_*)
 */
void nlang_template_status_query(nlang_expression_t* expr,
                                 nlang_query_t query_type);

/**
 * @brief Build acknowledgment response
 * @param expr Output expression
 * @param success Whether action succeeded
 * @param emotion Response emotion
 */
void nlang_template_acknowledge(nlang_expression_t* expr,
                                bool success,
                                nlang_pad_emotion_t emotion);

/**
 * @brief Build "help needed" request
 * @param expr Output expression
 * @param urgency Urgency level
 */
void nlang_template_help_request(nlang_expression_t* expr,
                                 nlang_pad_emotion_t urgency);

//=============================================================================
// Bandwidth Statistics
//=============================================================================

/**
 * @brief Expression bandwidth statistics
 */
typedef struct {
    size_t raw_bytes;                 // Serialized expression size
    size_t equivalent_text_bytes;     // Estimated plain text equivalent
    float compression_ratio;          // text/raw ratio
    uint32_t primitives_used;         // Number of primitives
} nlang_bandwidth_stats_t;

/**
 * @brief Calculate bandwidth statistics
 * @param expr Expression to analyze
 * @param stats Output statistics
 */
void nlang_calculate_bandwidth_stats(const nlang_expression_t* expr,
                                     nlang_bandwidth_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_NEURAL_LANGUAGE_H
