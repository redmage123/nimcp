//=============================================================================
// nimcp_nlp_cortical_adapter.h - NLP Cortical Integration API
//=============================================================================
/**
 * @file nimcp_nlp_cortical_adapter.h
 * @brief Routes NLP through speech/audio cortices for biological integration
 *
 * WHAT: API for cortical processing of network messages
 * WHY:  Enable brain to "hear" incoming and "speak" outgoing messages
 * HOW:  Connect NLP to audio_cortex (input) and speech_cortex (output)
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#ifndef NIMCP_NLP_CORTICAL_ADAPTER_H
#define NIMCP_NLP_CORTICAL_ADAPTER_H

#include "networking/nlp/nimcp_neural_link_protocol.h"
#include "core/brain/nimcp_brain.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Opaque Types
//=============================================================================

typedef struct nlp_cortical_adapter_struct nlp_cortical_adapter_t;

//=============================================================================
// Lifecycle
//=============================================================================

/**
 * @brief Create cortical adapter for NLP node
 * @param node NLP node to connect
 * @param brain Brain containing audio/speech cortices
 * @return Adapter or NULL on failure
 */
nlp_cortical_adapter_t* nlp_cortical_adapter_create(nlp_node_t node, brain_t brain);

/**
 * @brief Destroy cortical adapter
 * @param adapter Adapter to destroy
 */
void nlp_cortical_adapter_destroy(nlp_cortical_adapter_t* adapter);

//=============================================================================
// Message Processing
//=============================================================================

/**
 * @brief Process received message through audio cortex ("hear" message)
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
    size_t payload_len
);

/**
 * @brief Prepare message through speech cortex ("speak" message)
 * @param adapter Cortical adapter
 * @param msg_type Message type
 * @param priority Priority level
 * @param payload Original payload
 * @param payload_len Payload length
 * @param modulated_payload Output buffer
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
    size_t* modulated_len
);

//=============================================================================
// Modulation
//=============================================================================

/**
 * @brief Update neuromodulation state
 * @param adapter Cortical adapter
 */
void nlp_cortical_update_modulation(nlp_cortical_adapter_t* adapter);

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Get cortical processing statistics
 * @param adapter Cortical adapter
 * @param heard Output: messages heard
 * @param spoken Output: messages spoken
 * @param alerts Output: priority alerts
 */
void nlp_cortical_get_stats(
    nlp_cortical_adapter_t* adapter,
    uint64_t* heard,
    uint64_t* spoken,
    uint64_t* alerts
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_NLP_CORTICAL_ADAPTER_H
