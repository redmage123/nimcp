//=============================================================================
// nimcp_nlp_internal.h - Internal types for Neural Link Protocol
//=============================================================================
/**
 * @file nimcp_nlp_internal.h
 * @brief Internal types and structures for NLP implementation
 *
 * WHAT: Internal types not exposed in public API
 * WHY:  Keep crypto state and other implementation details private
 * HOW:  Include only in .c files, not in public headers
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#ifndef NIMCP_NLP_INTERNAL_H
#define NIMCP_NLP_INTERNAL_H

#include "networking/nlp/nimcp_neural_link_protocol.h"
#include "utils/thread/nimcp_thread.h"
#include <stdbool.h>
#include <stdint.h>
#include <netinet/in.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Crypto State
//=============================================================================

/**
 * @brief Cryptographic state for NLP node
 *
 * WHAT: Contains all crypto-related state for a node
 * WHY:  Centralize crypto state management
 * HOW:  Allocated and managed by nlp_crypto_init/shutdown
 */
typedef struct {
    // Initialization state
    bool initialized;                /**< Crypto subsystem initialized */
    bool use_openssl;                /**< Using OpenSSL (vs libsodium) */

    // HKDF salt for key derivation
    uint8_t hkdf_salt[32];           /**< Salt for HKDF */

    // RNG state
    uint64_t rng_counter;            /**< Nonce counter */
    uint32_t rng_seed;               /**< PRNG seed (fallback only) */

    // Statistics
    uint64_t nonces_generated;       /**< Number of nonces generated */
    uint64_t encryptions_performed;  /**< Number of encryption operations */
    uint64_t decryptions_performed;  /**< Number of decryption operations */
    uint64_t crypto_errors;          /**< Number of crypto errors */
} nlp_crypto_state_t;

//=============================================================================
// Node Internal Structure
//=============================================================================

/**
 * @brief Internal node structure (opaque handle implementation)
 *
 * WHAT: Complete state for an NLP node
 * WHY:  Encapsulate all node state for thread-safe operation
 * HOW:  Access via opaque nlp_node_t handle
 */
struct nlp_node_struct {
    // Validation
    uint32_t magic;                      /**< Magic number for validation */

    // Configuration
    nlp_config_t config;                 /**< Node configuration */

    // Identity
    uint32_t brain_id;                   /**< Brain identifier */
    bool is_master;                      /**< True if master node */

    // Operating state (protected by state_mutex)
    nlp_mode_t current_mode;             /**< Current operating mode */
    nlp_emcon_level_t emcon_level;       /**< Current EMCON level */
    bool running;                        /**< Node is running */
    nimcp_mutex_t state_mutex;           /**< Protects running, threads_running, current_mode */

    // Network
    int socket_fd;                       /**< UDP socket */
    struct sockaddr_in bind_addr;        /**< Bound address */

    // Peer management
    nlp_peer_t peers[NLP_MAX_PEERS];     /**< Peer table */
    uint32_t peer_count;                 /**< Number of active peers */
    nimcp_mutex_t peer_mutex;            /**< Protects peer table */

    // Pre-shared keys
    nlp_key_slot_t psk_slots[NLP_KEY_SLOTS]; /**< PSK key slots */
    nimcp_mutex_t key_mutex;             /**< Protects key slots */

    // Environment metrics
    nlp_environment_t environment;       /**< Environment metrics */
    nimcp_mutex_t env_mutex;             /**< Protects environment */

    // Statistics
    nlp_stats_t stats;                   /**< Node statistics */
    nimcp_mutex_t stats_mutex;           /**< Protects stats */

    // Crypto state (for crypto module)
    nlp_crypto_state_t* crypto;          /**< Cryptographic state */

    // Callbacks
    nlp_message_callback_t message_callback;
    nlp_peer_callback_t peer_callback;
    nlp_mode_callback_t mode_callback;
    void* user_data;                     /**< User callback context */

    // Threading (using NIMCP thread utilities)
    nimcp_thread_t recv_thread;          /**< Receive thread */
    nimcp_thread_t heartbeat_thread;     /**< Heartbeat thread */
    nimcp_thread_t stealth_thread;       /**< Stealth mode thread */
    bool threads_running;                /**< Threads started flag */

    // Bio-async integration (optional - module-level context)
    void* bio_module_ctx;                /**< For bio-router integration */

    // Sequence numbers
    uint16_t tx_sequence;                /**< TX sequence counter */
    nimcp_mutex_t seq_mutex;             /**< Protects sequence counter */

    // Stealth mode state
    uint64_t next_burst_time;            /**< Next burst transmission time */
    uint8_t* burst_buffer;               /**< Burst data buffer */
    size_t burst_buffer_size;            /**< Buffer capacity */
    size_t burst_buffer_used;            /**< Buffer current usage */
    nimcp_mutex_t burst_mutex;           /**< Protects burst state */

    // Message queue for tactical/stealth modes
    nlp_message_t* pending_messages[256];/**< Pending message queue */
    uint32_t pending_head;               /**< Queue head index */
    uint32_t pending_tail;               /**< Queue tail index */
    nimcp_mutex_t queue_mutex;           /**< Protects message queue */
};

#define NLP_NODE_MAGIC 0x4E4C504E  // 'NLPN'

//=============================================================================
// Internal Functions (used by implementation files)
//=============================================================================

/**
 * @brief Initialize crypto subsystem
 * @param node NLP node
 * @return 0 on success, negative on error
 */
int nlp_crypto_init(nlp_node_t node);

/**
 * @brief Shutdown crypto subsystem
 * @param node NLP node
 */
void nlp_crypto_shutdown(nlp_node_t node);

/**
 * @brief Generate unique nonce
 * @param node NLP node
 * @param nonce Output buffer (NLP_NONCE_SIZE bytes)
 * @return 0 on success, negative on error
 */
int nlp_crypto_generate_nonce(nlp_node_t node, uint8_t* nonce);

/**
 * @brief Encrypt data with AES-256-GCM
 * @param node NLP node
 * @param key Session key (32 bytes)
 * @param nonce Nonce (12 bytes)
 * @param plaintext Data to encrypt
 * @param plaintext_len Plaintext length
 * @param aad Additional authenticated data
 * @param aad_len AAD length
 * @param ciphertext Output buffer
 * @param ciphertext_len Output buffer size
 * @param tag Authentication tag output (16 bytes)
 * @return Bytes written on success, negative on error
 */
int nlp_crypto_encrypt(
    nlp_node_t node,
    const uint8_t* key,
    const uint8_t* nonce,
    const uint8_t* plaintext,
    size_t plaintext_len,
    const uint8_t* aad,
    size_t aad_len,
    uint8_t* ciphertext,
    size_t ciphertext_len,
    uint8_t* tag);

/**
 * @brief Decrypt and verify data
 * @param node NLP node
 * @param key Session key (32 bytes)
 * @param nonce Nonce (12 bytes)
 * @param ciphertext Encrypted data
 * @param ciphertext_len Ciphertext length
 * @param aad Additional authenticated data
 * @param aad_len AAD length
 * @param tag Authentication tag (16 bytes)
 * @param plaintext Output buffer
 * @param plaintext_len Output buffer size
 * @return Bytes written on success, negative on error
 */
int nlp_crypto_decrypt(
    nlp_node_t node,
    const uint8_t* key,
    const uint8_t* nonce,
    const uint8_t* ciphertext,
    size_t ciphertext_len,
    const uint8_t* aad,
    size_t aad_len,
    const uint8_t* tag,
    uint8_t* plaintext,
    size_t plaintext_len);

/**
 * @brief Derive session key from shared secret using node's HKDF salt
 * @param node NLP node (provides HKDF salt from crypto state)
 * @param shared_secret ECDH shared secret
 * @param secret_len Secret length
 * @param info Context info for key derivation
 * @param info_len Info length
 * @param session_key Output key (32 bytes)
 * @return 0 on success, negative on error
 */
int nlp_crypto_derive_session_key(
    nlp_node_t node,
    const uint8_t* shared_secret,
    size_t secret_len,
    const uint8_t* info,
    size_t info_len,
    uint8_t* session_key);

/**
 * @brief Generate X25519 keypair
 * @param node NLP node (for RNG state)
 * @param public_key Output public key (32 bytes)
 * @param private_key Output private key (32 bytes)
 * @return 0 on success, negative on error
 */
int nlp_crypto_generate_keypair(
    nlp_node_t node,
    uint8_t* public_key,
    uint8_t* private_key);

/**
 * @brief Calculate CRC-16-CCITT
 * @param data Data to checksum
 * @param len Data length
 * @return CRC-16 value
 */
uint16_t nlp_crc16(const uint8_t* data, size_t len);

//=============================================================================
// Session Management (from nimcp_nlp_session.c)
//=============================================================================

/**
 * @brief Initialize session for peer
 */
int nlp_session_init(nlp_peer_t* peer);

/**
 * @brief Start handshake
 */
int nlp_session_start_handshake(nlp_node_t node, nlp_peer_t* peer);

/**
 * @brief Handle handshake request (implementation signature)
 */
int nlp_session_handle_handshake_req(nlp_node_t node, nlp_peer_t* peer,
                                      const nlp_header_t* header);

/**
 * @brief Handle handshake response (implementation signature)
 */
int nlp_session_handle_handshake_resp(nlp_node_t node, nlp_peer_t* peer,
                                       const nlp_header_t* header);

/**
 * @brief Handle handshake final (implementation signature)
 */
int nlp_session_handle_handshake_final(nlp_node_t node, nlp_peer_t* peer,
                                        const nlp_header_t* header);

/**
 * @brief Establish session
 */
int nlp_session_establish(nlp_node_t node, nlp_peer_t* peer);

/**
 * @brief Close session
 */
int nlp_session_close(nlp_node_t node, nlp_peer_t* peer);

/**
 * @brief Select valid PSK
 */
int nlp_session_select_psk(nlp_node_t node, uint8_t* slot_out);

/**
 * @brief Validate timestamp for replay attack prevention
 */
int nlp_session_validate_timestamp(const nlp_header_t* header, uint32_t window_sec);

/**
 * @brief Check sequence number
 */
int nlp_session_check_sequence(nlp_peer_t* peer, uint16_t sequence);

/**
 * @brief Rotate session key
 */
int nlp_session_key_rotation(nlp_node_t node, nlp_peer_t* peer);

//=============================================================================
// Peer Management (from nimcp_nlp_session.c)
//=============================================================================

nlp_peer_t* nlp_peer_add(nlp_node_t node, uint32_t peer_id,
                         const char* address, uint16_t port);
int nlp_peer_remove(nlp_node_t node, uint32_t peer_id);
nlp_peer_t* nlp_peer_find(nlp_node_t node, uint32_t peer_id);
nlp_peer_t* nlp_peer_find_by_address(nlp_node_t node, const char* address, uint16_t port);
int nlp_peer_update_stats(nlp_peer_t* peer, uint64_t bytes_sent, uint64_t bytes_received);
int nlp_peer_check_health(nlp_node_t node, nlp_peer_t* peer);

//=============================================================================
// Message Functions (from nimcp_nlp_message.c)
//=============================================================================

void nlp_header_init(nlp_header_t* header);
void nlp_header_serialize(nlp_header_t* header);
int nlp_header_deserialize(nlp_header_t* header);

nlp_message_t* nlp_message_create(nlp_msg_type_t msg_type, const void* payload_data, uint16_t payload_len);
void nlp_message_destroy(nlp_message_t* msg);

int nlp_message_serialize(const nlp_message_t* msg, uint8_t* buffer, size_t buffer_size, size_t* bytes_written);
int nlp_message_deserialize(const uint8_t* buffer, size_t buffer_size, nlp_message_t** msg);
int nlp_message_validate(const nlp_message_t* msg);

/**
 * @brief Pad message to fixed size for stealth mode
 */
int nlp_message_pad_to_fixed_size(const nlp_message_t* msg, uint8_t* padded_buffer);

/**
 * @brief Create chaff message for traffic analysis resistance
 */
nlp_message_t* nlp_message_create_chaff(uint32_t sender_id, uint32_t dest_id);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_NLP_INTERNAL_H
