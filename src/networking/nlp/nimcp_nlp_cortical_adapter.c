#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_nlp_cortical_adapter.c - Neural Link Protocol Cortical Integration
//=============================================================================
/**
 * @file nimcp_nlp_cortical_adapter.c
 * @brief Routes NLP messages through speech and audio cortices
 *
 * WHAT: Biologically-inspired network I/O through cortical processing
 * WHY:  Enable the brain to "hear" incoming messages and "speak" outgoing ones
 * HOW:  Audio cortex processes received messages; speech cortex produces outgoing
 *
 * DESIGN PHILOSOPHY:
 * The brain doesn't directly read network packets - it "hears" them through
 * the audio cortex, which extracts meaningful patterns like tone, urgency,
 * and semantic content. Similarly, outgoing messages are "spoken" through
 * Broca's area and speech motor cortex, allowing emotional modulation.
 *
 * INFORMATION FLOW:
 *
 * RECEIVING (Auditory Pathway):
 * ┌─────────────┐    ┌──────────────┐    ┌───────────────┐    ┌────────────┐
 * │ Network RX  │───→│ Audio Cortex │───→│ Wernicke's    │───→│ Semantic   │
 * │ (raw bytes) │    │ (frequency)  │    │ (comprehend)  │    │ Processing │
 * └─────────────┘    └──────────────┘    └───────────────┘    └────────────┘
 *
 * TRANSMITTING (Speech Pathway):
 * ┌────────────┐    ┌──────────────┐    ┌───────────────┐    ┌─────────────┐
 * │ Semantic   │───→│ Broca's Area │───→│ Speech Motor  │───→│ Network TX  │
 * │ Intent     │    │ (structure)  │    │ (articulate)  │    │ (raw bytes) │
 * └────────────┘    └──────────────┘    └───────────────┘    └─────────────┘
 *
 * CORTICAL MODULATION:
 * - Dopamine: Controls "speaking" rate (high DA = rapid TX)
 * - Acetylcholine: Controls "listening" sensitivity (high ACh = sharp discrimination)
 * - Norepinephrine: Controls alertness to priority messages
 * - Serotonin: Controls emotional tone of outgoing messages
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include "networking/nlp/nimcp_neural_link_protocol.h"
#include "networking/nlp/nimcp_nlp_internal.h"
#include "perception/nimcp_audio_cortex.h"
#include "perception/nimcp_speech_cortex.h"
#include "core/brain/regions/broca/nimcp_broca_adapter.h"
#include "core/brain/nimcp_brain.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "api/nimcp_api_exception.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "security/nimcp_bbb_helpers.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(nlp_cortical_adapter)

//=============================================================================
// Module Constants
//=============================================================================

#define NLP_CORTICAL_MODULE "nlp_cortical_adapter"

// Frequency mapping for message types (like tonal language)
#define NLP_FREQ_HANDSHAKE      440.0f   // A4 - neutral greeting
#define NLP_FREQ_SPIKE_BATCH    880.0f   // A5 - high activity
#define NLP_FREQ_EMERGENCY     1760.0f   // A6 - urgent alert
#define NLP_FREQ_HEARTBEAT      220.0f   // A3 - low, calm
#define NLP_FREQ_STEALTH        110.0f   // A2 - whisper
#define NLP_FREQ_DATA           330.0f   // E4 - information

// Prosodic features for emotional encoding
#define PROSODY_NORMAL          1.0f
#define PROSODY_URGENT          1.5f     // Faster, higher pitch
#define PROSODY_CALM            0.7f     // Slower, lower pitch
#define PROSODY_STEALTHY        0.3f     // Whisper speed

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Cortical adapter context
 */
typedef struct {
    // Cortical references
    audio_cortex_t* audio_cortex;       /**< For receiving messages */
    speech_cortex_t* speech_cortex;     /**< For transmitting messages */
    brain_t brain;                      /**< For neuromodulation */

    // NLP node reference
    nlp_node_t nlp_node;

    // Bio-async integration (optional)
    void* inbox;  // Reserved for future bio-async inbox
    bool bio_async_enabled;

    // Processing parameters
    float receive_sensitivity;          /**< ACh-modulated listening */
    float transmit_rate;               /**< DA-modulated speaking rate */
    float attention_level;              /**< NE-modulated alertness */
    float emotional_valence;           /**< 5HT-modulated tone */

    // Statistics
    uint64_t messages_heard;
    uint64_t messages_spoken;
    uint64_t priority_alerts;
} nlp_cortical_adapter_t;

// Global adapter instance per node
static nlp_cortical_adapter_t* g_adapter = NULL;

//=============================================================================
// Forward Declarations
//=============================================================================

static float nlp_message_to_frequency(nlp_msg_type_t type, nlp_priority_t priority);
static float nlp_get_prosody(nlp_priority_t priority, nlp_mode_t mode);
static void nlp_cortical_bio_handler(void* context, const bio_message_header_t* msg);

//=============================================================================
// Cortical Adapter Lifecycle
//=============================================================================

/**
 * @brief Create cortical adapter for NLP node
 *
 * WHAT: Create adapter linking NLP to speech/audio cortices
 * WHY:  Enable biologically-inspired network I/O
 * HOW:  Connect NLP node to existing cortical structures in brain
 *
 * @param node NLP node to adapt
 * @param brain Brain containing cortices
 * @return Adapter or NULL on failure
 */
nlp_cortical_adapter_t* nlp_cortical_adapter_create(nlp_node_t node, brain_t brain) {
    if (!bbb_check_pointer(node, "nlp_cortical_adapter_create") ||
        !bbb_check_pointer(brain, "nlp_cortical_adapter_create")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nlp_cortical_adapter_create: bbb_check_pointer is NULL");
        return NULL;
    }

    nlp_cortical_adapter_t* adapter = (nlp_cortical_adapter_t*)
        nimcp_calloc(1, sizeof(nlp_cortical_adapter_t));
    if (!adapter) {
        LOG_MODULE_ERROR(NLP_CORTICAL_MODULE, "Failed to allocate adapter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adapter is NULL");

        return NULL;
    }

    adapter->nlp_node = node;
    adapter->brain = brain;

    // Note: Audio/speech cortices would be obtained from brain if available
    // For now, create default instances if needed
    adapter->audio_cortex = NULL;
    adapter->speech_cortex = NULL;

    // Create default audio cortex for message processing
    LOG_MODULE_INFO(NLP_CORTICAL_MODULE, "Creating default audio cortex");
    audio_cortex_config_t audio_cfg = {
        .sample_rate = 16000,
        .frame_size = 512,
        .num_mel_filters = 40,
        .num_mfcc = 13,
        .num_freq_bins = 256,
        .feature_dim = 64,
        .enable_memory = true,
        .enable_bio_async = false
    };
    adapter->audio_cortex = audio_cortex_create(&audio_cfg);

    // Note: speech_cortex would be created here if header available
    // For now, speech functionality is handled directly
    (void)adapter->speech_cortex;  // Suppress unused warning

    // Initialize modulation parameters to neutral
    adapter->receive_sensitivity = 1.0F;
    adapter->transmit_rate = 1.0F;
    adapter->attention_level = 1.0F;
    adapter->emotional_valence = 0.5F;  // Neutral

    // Bio-async integration - module context initialized on first use
    adapter->bio_async_enabled = false;  // Enable when bio-router available
    adapter->inbox = NULL;  // No direct inbox - uses bio_router_process_inbox

    g_adapter = adapter;

    LOG_MODULE_INFO(NLP_CORTICAL_MODULE, "Cortical adapter created");
    bbb_audit_log(BBB_AUDIT_INFO, NLP_CORTICAL_MODULE, "adapter_created", "brain=%p", brain);

    return adapter;
}

/**
 * @brief Destroy cortical adapter
 */
void nlp_cortical_adapter_destroy(nlp_cortical_adapter_t* adapter) {
    if (!adapter) return;

    // Note: bio-router module unregistration would happen here if registered

    // Clear global pointer BEFORE freeing to prevent use-after-free
    if (g_adapter == adapter) {
        g_adapter = NULL;
    }

    // Destroy audio cortex if we own it
    if (adapter->audio_cortex) {
        audio_cortex_destroy(adapter->audio_cortex);
        adapter->audio_cortex = NULL;
    }

    nimcp_free(adapter);

    LOG_MODULE_INFO(NLP_CORTICAL_MODULE, "Cortical adapter destroyed");
}

//=============================================================================
// Neuromodulation Updates
//=============================================================================

/**
 * @brief Update cortical processing parameters from neuromodulation
 *
 * WHAT: Read neuromodulator levels and adjust processing
 * WHY:  Emotional state affects how we "hear" and "speak"
 * HOW:  Map DA/ACh/NE/5HT to rate/sensitivity/attention/valence
 */
void nlp_cortical_update_modulation(nlp_cortical_adapter_t* adapter) {
    if (!adapter || !adapter->brain) return;

    neuromodulator_system_t neuromod = brain_get_neuromodulator_system(adapter->brain);
    if (!neuromod) return;

    // Dopamine → Transmit rate (speech fluency)
    // High DA = rapid, eager communication
    float da = neuromodulator_get_level(neuromod, NEUROMOD_DOPAMINE);
    adapter->transmit_rate = 0.6F + da * 0.8F;  // [0.6, 1.4]

    // Acetylcholine → Receive sensitivity (listening acuity)
    // High ACh = sharp discrimination, catches subtle differences
    float ach = neuromodulator_get_level(neuromod, NEUROMOD_ACETYLCHOLINE);
    adapter->receive_sensitivity = 0.6F + ach * 0.8F;  // [0.6, 1.4]

    // Norepinephrine → Attention/alertness
    // High NE = hypervigilant, notices priority messages faster
    float ne = neuromodulator_get_level(neuromod, NEUROMOD_NOREPINEPHRINE);
    adapter->attention_level = 0.5F + ne;  // [0.5, 1.5]

    // Serotonin → Emotional valence of output
    // High 5HT = positive/warm tone; Low = cold/neutral
    float serotonin = neuromodulator_get_level(neuromod, NEUROMOD_SEROTONIN);
    adapter->emotional_valence = serotonin;  // [0, 1]
}

//=============================================================================
// Receiving Messages (Auditory Pathway)
//=============================================================================

/**
 * @brief Process received NLP message through audio cortex
 *
 * WHAT: Convert raw network message to cortical representation
 * WHY:  Brain "hears" messages, enabling emotional/attention processing
 * HOW:  Map message properties to auditory features, process through A1
 *
 * AUDITORY ENCODING:
 * - Message type → Fundamental frequency (pitch)
 * - Priority → Volume/amplitude
 * - Mode → Timbre (standard=clear, tactical=clipped, stealth=whisper)
 * - Payload size → Duration
 * - Sender ID → Voice identity (distinct "voice" per peer)
 *
 * @param adapter Cortical adapter
 * @param header Message header
 * @param payload Decrypted payload
 * @param payload_len Payload length
 * @return 0 on success
 */
int nlp_cortical_hear_message(
    nlp_cortical_adapter_t* adapter,
    const nlp_header_t* header,
    const uint8_t* payload,
    size_t payload_len)
{
    if (!adapter || !header) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nlp_cortical_hear_message: required parameter is NULL (adapter, header)");
        return -1;
    }

    // Update neuromodulation
    nlp_cortical_update_modulation(adapter);

    // Extract message properties
    nlp_msg_type_t msg_type = (nlp_msg_type_t)ntohs(header->msg_type);
    nlp_priority_t priority = (nlp_priority_t)NLP_GET_PRIORITY(header);
    nlp_mode_t mode = (nlp_mode_t)NLP_GET_MODE(header);
    uint32_t sender_id = ntohl(header->sender_id);

    // Convert to auditory features
    float frequency = nlp_message_to_frequency(msg_type, priority);
    float amplitude = 0.3F + (priority / 3.0F) * 0.7F;  // [0.3, 1.0]
    float duration_ms = 50.0F + (payload_len / 100.0F) * 50.0F;  // [50, ~ms]

    // Apply receive sensitivity modulation
    amplitude *= adapter->receive_sensitivity;

    // Apply attention modulation for priority messages
    if (priority >= NLP_PRIORITY_HIGH) {
        amplitude *= adapter->attention_level;
        adapter->priority_alerts++;

        // Send norepinephrine burst for urgent messages
        // Note: neuromodulator_system_release would be used here if available
        (void)adapter->brain;  // Suppress unused warning
    }

    // Generate synthetic audio signal for cortical processing
    // This creates a "tonal signature" the brain can recognize
    if (adapter->audio_cortex) {
        // Create a simple tone representing the message
        uint32_t sample_rate = 16000;
        uint32_t num_samples = (uint32_t)(duration_ms * sample_rate / 1000.0F);
        if (num_samples > 1600) num_samples = 1600;  // Cap at 100ms

        float* audio_signal = (float*)nimcp_calloc(num_samples, sizeof(float));
        if (audio_signal) {
            // Generate tone with harmonics based on mode
            for (uint32_t i = 0; i < num_samples; i++) {
                float t = (float)i / sample_rate;
                float sample = amplitude * sinf(2.0F * M_PI * frequency * t);

                // Add harmonics based on mode (timbre)
                if (mode == NLP_MODE_STANDARD) {
                    // Rich harmonics - clear voice
                    sample += 0.3F * amplitude * sinf(2.0F * M_PI * frequency * 2.0F * t);
                    sample += 0.1F * amplitude * sinf(2.0F * M_PI * frequency * 3.0F * t);
                } else if (mode == NLP_MODE_TACTICAL) {
                    // Clipped harmonics - radio voice
                    sample = fmaxf(-0.8F, fminf(0.8F, sample * 1.5F));
                }
                // Stealth mode: pure tone only (whisper-like)

                audio_signal[i] = sample;
            }

            // Process through audio cortex (mono audio, no output features needed)
            float features[64];  // Output features buffer
            audio_cortex_process(adapter->audio_cortex, audio_signal, num_samples, 1, features);

            nimcp_free(audio_signal);
        }
    }

    adapter->messages_heard++;

    // Log received message (bio-async broadcast would require module context)
    LOG_MODULE_DEBUG(NLP_CORTICAL_MODULE, "Heard message: type=%s freq=%.0fHz amp=%.2f",
                     nlp_msg_type_name(msg_type), frequency, amplitude);

    return 0;
}

//=============================================================================
// Transmitting Messages (Speech Pathway)
//=============================================================================

/**
 * @brief Prepare message for transmission through speech cortex
 *
 * WHAT: Apply cortical processing to outgoing message
 * WHY:  Allow emotional state to modulate communication style
 * HOW:  Use Broca's area for structure, speech motor for articulation
 *
 * SPEECH ENCODING:
 * - Emotional valence → Word choice emphasis
 * - Transmit rate → Message pacing
 * - Priority → Prosodic emphasis
 * - Mode → Speaking style (clear/clipped/whisper)
 *
 * @param adapter Cortical adapter
 * @param msg_type Message type
 * @param priority Priority level
 * @param payload Payload to send
 * @param payload_len Payload length
 * @param modulated_payload Output buffer for processed payload
 * @param modulated_len Output length
 * @return 0 on success
 */
int nlp_cortical_speak_message(
    nlp_cortical_adapter_t* adapter,
    nlp_msg_type_t msg_type,
    nlp_priority_t priority,
    const uint8_t* payload,
    size_t payload_len,
    uint8_t* modulated_payload,
    size_t* modulated_len)
{
    if (!adapter) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adapter is NULL");

        return -1;

    }

    // Update neuromodulation
    nlp_cortical_update_modulation(adapter);

    // Get prosodic parameters based on priority and mode
    nlp_mode_t mode = nlp_get_mode(adapter->nlp_node);
    float prosody = nlp_get_prosody(priority, mode);

    // Apply transmit rate modulation
    prosody *= adapter->transmit_rate;

    // Modulate based on emotional valence
    // High valence (happy) → slightly faster, higher pitch encoding
    // Low valence (stressed) → slower, more deliberate
    if (adapter->emotional_valence > 0.6F) {
        prosody *= 1.1F;
    } else if (adapter->emotional_valence < 0.4F) {
        prosody *= 0.9F;
    }

    // Process through speech cortex if available
    if (adapter->speech_cortex) {
        // The speech cortex adds phonological structure
        // For network protocol, this means adding rhythm/timing markers

        // Create prosodic header (prepended to payload)
        typedef struct {
            uint8_t prosody_code;      // Encoded prosody level
            uint8_t emotional_code;    // Encoded emotion
            uint16_t timing_hint;      // Suggested inter-message delay
        } prosody_header_t;

        prosody_header_t ph;
        ph.prosody_code = (uint8_t)(prosody * 100);  // 0-200
        ph.emotional_code = (uint8_t)(adapter->emotional_valence * 255);
        ph.timing_hint = htons((uint16_t)(100 / prosody));  // ms

        // Copy prosody header + original payload
        if (modulated_payload && modulated_len) {
            memcpy(modulated_payload, &ph, sizeof(ph));
            if (payload && payload_len > 0) {
                memcpy(modulated_payload + sizeof(ph), payload, payload_len);
            }
            *modulated_len = sizeof(ph) + payload_len;
        }
    } else {
        // No speech cortex - pass through unchanged
        if (modulated_payload && modulated_len && payload && payload_len > 0) {
            memcpy(modulated_payload, payload, payload_len);
            *modulated_len = payload_len;
        }
    }

    adapter->messages_spoken++;

    // Send dopamine pulse for successful speech production
    // Note: neuromodulator_system_release would be used here if available
    (void)priority;  // Suppress unused warning

    // Send bio-async message about outgoing communication
    // Note: Bio-async broadcast requires module context, which we may not have
    // Log the outgoing message instead
    LOG_MODULE_DEBUG(NLP_CORTICAL_MODULE, "Spoke message: type=%s prosody=%.2f emotion=%.2f",
                     nlp_msg_type_name(msg_type), prosody, adapter->emotional_valence);

    return 0;
}

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Map message type to auditory frequency
 */
static float nlp_message_to_frequency(nlp_msg_type_t type, nlp_priority_t priority) {
    float base_freq;

    // Map message categories to frequency ranges
    uint16_t category = type & 0xFF00;

    switch (category) {
        case 0x0000:  // Session management
            base_freq = NLP_FREQ_HANDSHAKE;
            break;

        case 0x0100:  // Neural sync
            base_freq = NLP_FREQ_SPIKE_BATCH;
            break;

        case 0x0200:  // Swarm coordination
            base_freq = NLP_FREQ_HEARTBEAT;
            break;

        case 0x0300:  // Tactical/Emergency
            base_freq = NLP_FREQ_EMERGENCY;
            break;

        case 0x0400:  // Stealth
            base_freq = NLP_FREQ_STEALTH;
            break;

        case 0x0500:  // Disaster/SAR
            base_freq = NLP_FREQ_DATA;
            break;

        default:
            base_freq = NLP_FREQ_DATA;
    }

    // Modulate by priority (higher priority = higher pitch)
    base_freq *= (1.0F + priority * 0.25F);

    return base_freq;
}

/**
 * @brief Get prosodic rate for priority and mode
 */
static float nlp_get_prosody(nlp_priority_t priority, nlp_mode_t mode) {
    float base_prosody = PROSODY_NORMAL;

    // Mode affects base prosody
    switch (mode) {
        case NLP_MODE_STANDARD:
            base_prosody = PROSODY_NORMAL;
            break;

        case NLP_MODE_TACTICAL:
            base_prosody = PROSODY_URGENT;  // Faster in tactical
            break;

        case NLP_MODE_STEALTH:
            base_prosody = PROSODY_STEALTHY;  // Slow whisper
            break;
    }

    // Priority adds urgency
    switch (priority) {
        case NLP_PRIORITY_LOW:
            base_prosody *= PROSODY_CALM;
            break;

        case NLP_PRIORITY_NORMAL:
            // No change
            break;

        case NLP_PRIORITY_HIGH:
            base_prosody *= 1.2F;
            break;

        case NLP_PRIORITY_CRITICAL:
            base_prosody *= PROSODY_URGENT;
            break;
    }

    return base_prosody;
}

/**
 * @brief Bio-async message handler
 */
static void nlp_cortical_bio_handler(void* context, const bio_message_header_t* msg) {
    nlp_cortical_adapter_t* adapter = (nlp_cortical_adapter_t*)context;
    if (!adapter || !msg) return;

    // Handle neuromodulator signals that affect communication
    switch (msg->type) {
        case BIO_MSG_NEUROMODULATOR_RELEASE:
            // Update our modulation state
            nlp_cortical_update_modulation(adapter);
            break;

        case BIO_MSG_ATTENTION_SHIFT:
            // Attention shifted - increase listening sensitivity
            adapter->receive_sensitivity *= 1.2F;
            if (adapter->receive_sensitivity > 2.0F) {
                adapter->receive_sensitivity = 2.0F;
            }
            break;

        case BIO_MSG_CURIOSITY_SIGNAL:
            // Curiosity signal - affects listening and exploration
            // (Could increase receive sensitivity or trigger questions)
            break;

        default:
            break;
    }
}

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Get cortical adapter statistics
 */
void nlp_cortical_get_stats(
    nlp_cortical_adapter_t* adapter,
    uint64_t* heard,
    uint64_t* spoken,
    uint64_t* alerts)
{
    if (!adapter) return;

    if (heard) *heard = adapter->messages_heard;
    if (spoken) *spoken = adapter->messages_spoken;
    if (alerts) *alerts = adapter->priority_alerts;
}
