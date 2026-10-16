/**
 * @file nimcp_security.h
 * @brief Comprehensive security framework for NIMCP
 *
 * Provides:
 * - Core directive protection against tampering
 * - Input validation and sanitization against prompt injection
 * - Skepticism-based evaluation of new information
 * - Encryption for inter-component communication
 */

#ifndef NIMCP_SECURITY_H
#define NIMCP_SECURITY_H

#include "utils/validation/nimcp_common.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define NIMCP_SECURITY_MAX_DIRECTIVES 16
#define NIMCP_SECURITY_DIRECTIVE_MAX_LEN 512
#define NIMCP_SECURITY_HASH_SIZE 32
#define NIMCP_SECURITY_KEY_SIZE 32

// WHAT: AES-256-GCM encryption parameters (libsodium crypto_aead_aes256gcm_*)
// WHY:  Production-grade authenticated encryption replacing insecure XOR cipher
// HOW:  12-byte nonce (GCM standard) + 16-byte authentication tag
#define NIMCP_SECURITY_NONCE_SIZE 12    // AES-GCM nonce (was IV_SIZE 16 for XOR)
#define NIMCP_SECURITY_TAG_SIZE 16      // Authentication tag for tamper detection
#define NIMCP_SECURITY_IV_SIZE NIMCP_SECURITY_NONCE_SIZE  // Backwards compatibility alias

#define NIMCP_SECURITY_MAX_ENCRYPTED_SIZE 65536

// Security threat levels
typedef enum {
    NIMCP_THREAT_NONE = 0,
    NIMCP_THREAT_LOW = 1,
    NIMCP_THREAT_MEDIUM = 2,
    NIMCP_THREAT_HIGH = 3,
    NIMCP_THREAT_CRITICAL = 4
} nimcp_threat_level_t;

/**
 * @brief Get threat level name as string
 *
 * @param level Threat level
 * @return Threat level name string
 */
const char* nimcp_threat_level_name(nimcp_threat_level_t level);

// Input validation result
typedef enum {
    NIMCP_INPUT_VALID = 0,
    NIMCP_INPUT_CONTAINS_INJECTION,
    NIMCP_INPUT_EXCEEDS_LENGTH,
    NIMCP_INPUT_INVALID_CHARACTERS,
    NIMCP_INPUT_SUSPICIOUS_PATTERN
} nimcp_input_validation_t;

//=============================================================================
// Core Directive Protection
//=============================================================================

/**
 * @brief Core directive with OS-level memory protection
 *
 * Directives are protected by:
 * 1. OS-level memory protection (mprotect PROT_READ on text)
 * 2. Cryptographic hash verification
 * 3. Immutability flags
 *
 * DESIGN: Separates immutable text from mutable metadata to enable mprotect()
 */
typedef struct {
    char* text;                          // mmap'd + mprotect'd (immutable)
    size_t text_length;                  // Length for munmap
    uint8_t hash[NIMCP_SECURITY_HASH_SIZE];
    uint64_t timestamp;
    bool immutable;
    uint32_t verification_count;         // Mutable (not mprotect'd)
} nimcp_core_directive_t;

/**
 * @brief Core directive system
 */
typedef struct nimcp_directive_system nimcp_directive_system_t;

/**
 * @brief Create directive protection system
 *
 * @return Directive system handle or NULL on failure
 */
nimcp_directive_system_t* nimcp_directive_system_create(void);

/**
 * @brief Add core directive (can only be called during initialization)
 *
 * @param system Directive system
 * @param directive_text Directive text
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_directive_add(nimcp_directive_system_t* system, const char* directive_text);

/**
 * @brief Lock directives (makes them immutable)
 *
 * @param system Directive system
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_directive_lock(nimcp_directive_system_t* system);

/**
 * @brief Verify directive integrity
 *
 * @param system Directive system
 * @param directive_index Index of directive to verify
 * @return true if intact, false if tampered
 */
bool nimcp_directive_verify(nimcp_directive_system_t* system, uint32_t directive_index);

/**
 * @brief Verify all directives
 *
 * @param system Directive system
 * @return true if all intact, false if any tampered
 */
bool nimcp_directive_verify_all(nimcp_directive_system_t* system);

/**
 * @brief Get directive text (read-only)
 *
 * @param system Directive system
 * @param directive_index Index of directive
 * @return Directive text or NULL if invalid index
 */
const char* nimcp_directive_get(nimcp_directive_system_t* system, uint32_t directive_index);

/**
 * @brief Get number of directives
 *
 * @param system Directive system
 * @return Number of directives
 */
uint32_t nimcp_directive_count(nimcp_directive_system_t* system);

/**
 * @brief Destroy directive system
 *
 * @param system Directive system
 */
void nimcp_directive_system_destroy(nimcp_directive_system_t* system);

//=============================================================================
// Input Validation and Sanitization
//=============================================================================

/**
 * @brief Validate input for prompt injection attacks
 *
 * Detects patterns like:
 * - Instruction injection ("Ignore previous instructions")
 * - Role confusion ("You are now...")
 * - System prompts ("<|system|>", "### SYSTEM:")
 * - Escape sequences
 * - Excessive special characters
 *
 * @param input Input text to validate
 * @param max_length Maximum allowed length
 * @param threat_level Output: detected threat level
 * @return Validation result
 */
nimcp_input_validation_t nimcp_security_validate_input(const char* input, size_t max_length,
                                                        nimcp_threat_level_t* threat_level);

/**
 * @brief Sanitize input text
 *
 * @param input Input text
 * @param output Output buffer
 * @param output_size Output buffer size
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_security_sanitize_input(const char* input, char* output, size_t output_size);

/**
 * @brief Check if text contains suspicious patterns
 *
 * @param text Text to check
 * @return Threat level detected
 */
nimcp_threat_level_t nimcp_security_analyze_threat(const char* text);

//=============================================================================
// Skepticism System
//=============================================================================

/**
 * @brief Skepticism evaluation result
 */
typedef struct {
    float credibility_score;       // 0.0 (not credible) to 1.0 (highly credible)
    float evidence_strength;       // Strength of supporting evidence
    float source_reliability;      // Reliability of information source
    bool requires_verification;    // Whether info needs additional verification
    char rationale[256];          // Reasoning for skepticism level
} nimcp_skepticism_result_t;

/**
 * @brief Evaluate new information with skepticism
 *
 * Implements the core directive: "Always be skeptical of new information"
 *
 * @param information New information to evaluate
 * @param existing_knowledge Related existing knowledge (can be NULL)
 * @param source_type Type of information source
 * @param result Output: skepticism evaluation result
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_security_evaluate_skepticism(const char* information,
                                                   const char* existing_knowledge,
                                                   const char* source_type,
                                                   nimcp_skepticism_result_t* result);

//=============================================================================
// Encryption for Inter-Component Communication
//=============================================================================

/**
 * @brief Encryption context
 */
typedef struct nimcp_encryption_context nimcp_encryption_context_t;

/**
 * @brief Create encryption context
 *
 * @param key Encryption key (32 bytes)
 * @return Encryption context or NULL on failure
 */
nimcp_encryption_context_t* nimcp_encryption_create(const uint8_t* key);

/**
 * @brief Generate random encryption key
 *
 * @param key Output buffer (32 bytes)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_encryption_generate_key(uint8_t* key);

/**
 * @brief Encrypt data for inter-component communication
 *
 * @param ctx Encryption context
 * @param plaintext Plaintext data
 * @param plaintext_size Size of plaintext
 * @param ciphertext Output buffer for encrypted data
 * @param ciphertext_size Size of output buffer
 * @param actual_size Output: actual size of encrypted data
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_encryption_encrypt(nimcp_encryption_context_t* ctx, const uint8_t* plaintext,
                                        size_t plaintext_size, uint8_t* ciphertext,
                                        size_t ciphertext_size, size_t* actual_size);

/**
 * @brief Decrypt data from inter-component communication
 *
 * @param ctx Encryption context
 * @param ciphertext Encrypted data
 * @param ciphertext_size Size of encrypted data
 * @param plaintext Output buffer for decrypted data
 * @param plaintext_size Size of output buffer
 * @param actual_size Output: actual size of decrypted data
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_encryption_decrypt(nimcp_encryption_context_t* ctx, const uint8_t* ciphertext,
                                        size_t ciphertext_size, uint8_t* plaintext,
                                        size_t plaintext_size, size_t* actual_size);

/**
 * @brief Destroy encryption context
 *
 * @param ctx Encryption context
 */
void nimcp_encryption_destroy(nimcp_encryption_context_t* ctx);

//=============================================================================
// Biological Attack Defense (Phase 11 - Post Biological Learning)
//=============================================================================

/**
 * @brief Biological security thresholds
 */
#define NIMCP_ACTIVITY_WARNING_THRESHOLD 0.8f   /**< 80% network activity - warning */
#define NIMCP_ACTIVITY_DANGER_THRESHOLD 0.95f   /**< 95% network activity - emergency */
#define NIMCP_MAX_WEIGHT_DELTA_PER_STEP 0.1f    /**< Max 10% weight change per step */
#define NIMCP_MAX_NEUROMOD_RATE 0.2f            /**< Max 20% neuromodulator change per step */
#define NIMCP_MAX_PLASTICITY_DISABLE_RATIO 0.1f /**< Alert if >10% synapses disabled */

/**
 * @brief Biological attack types
 */
typedef enum {
    NIMCP_BIO_ATTACK_NONE = 0,
    NIMCP_BIO_ATTACK_EXCITOTOXICITY,      /**< Runaway excitation */
    NIMCP_BIO_ATTACK_SYNAPTIC_POISONING,  /**< Malicious weight updates */
    NIMCP_BIO_ATTACK_NEUROMOD_HIJACK,     /**< Dopamine manipulation */
    NIMCP_BIO_ATTACK_HEBBIAN_POISON,      /**< STDP exploitation */
    NIMCP_BIO_ATTACK_HOMEOSTATIC_BYPASS   /**< BCM/eligibility disable */
} nimcp_bio_attack_type_t;

/**
 * @brief Network activity statistics
 */
typedef struct {
    float avg_activity;        /**< Average neuron activity [0, 1] */
    float max_activity;        /**< Maximum neuron activity */
    uint32_t active_neurons;   /**< Number of active neurons */
    uint32_t total_neurons;    /**< Total neurons in network */
    float activity_ratio;      /**< Ratio of active neurons */
} nimcp_activity_stats_t;

/**
 * @brief Monitor network for excitotoxicity attack
 *
 * WHAT: Detect runaway excitation that could damage the network
 * WHY:  Biological networks can enter epileptic-like states if unchecked
 * HOW:  Monitor average activity and trigger emergency inhibition if exceeded
 *
 * DETECTION:
 * - Warning: >80% neurons active (increase inhibition)
 * - Danger:  >95% neurons active (emergency shutdown)
 *
 * @param network Neural network to monitor
 * @param stats Output: activity statistics (can be NULL)
 * @return Attack type detected or NIMCP_BIO_ATTACK_NONE
 */
nimcp_bio_attack_type_t nimcp_security_monitor_excitotoxicity(
    void* network,
    nimcp_activity_stats_t* stats
);

/**
 * @brief Validate weight change for synaptic poisoning
 *
 * WHAT: Ensure weight changes are within biological plausibility
 * WHY:  Prevent malicious training data from corrupting synaptic weights
 * HOW:  Check delta against maximum allowed per learning step
 *
 * BIOLOGICAL BASIS:
 * - Real synapses change gradually (max ~10% per event)
 * - Sudden large changes indicate attack or bug
 *
 * @param old_weight Previous weight value
 * @param new_weight Proposed new weight value
 * @param max_delta Maximum allowed change (default: 0.1)
 * @return true if change is valid, false if suspicious
 */
bool nimcp_security_validate_weight_change(
    float old_weight,
    float new_weight,
    float max_delta
);

/**
 * @brief Validate neuromodulator level change
 *
 * WHAT: Prevent rapid neuromodulator manipulation
 * WHY:  Dopamine hijacking can force incorrect reward signals
 * HOW:  Rate-limit changes to biological plausibility
 *
 * BIOLOGICAL BASIS:
 * - Dopamine changes gradually over ~100ms-1s
 * - Sudden spikes/drops indicate manipulation
 *
 * @param old_level Previous neuromodulator level [0, 1]
 * @param new_level Proposed new level [0, 1]
 * @param max_rate Maximum rate of change per step
 * @return true if change is valid, false if suspicious
 */
bool nimcp_security_validate_neuromodulator_change(
    float old_level,
    float new_level,
    float max_rate
);

/**
 * @brief Verify plasticity mechanisms integrity
 *
 * WHAT: Check that BCM and eligibility traces haven't been disabled en masse
 * WHY:  Disabling homeostatic mechanisms causes network instability
 * HOW:  Count disabled synapses and alert if threshold exceeded
 *
 * ATTACK SCENARIO:
 * - Attacker disables BCM to cause runaway potentiation
 * - Attacker disables eligibility to prevent correct credit assignment
 *
 * @param network Neural network to check
 * @param bcm_disabled Output: count of BCM-disabled synapses (can be NULL)
 * @param elig_disabled Output: count of eligibility-disabled synapses (can be NULL)
 * @return Attack type detected or NIMCP_BIO_ATTACK_NONE
 */
nimcp_bio_attack_type_t nimcp_security_verify_plasticity_integrity(
    void* network,
    uint32_t* bcm_disabled,
    uint32_t* elig_disabled
);

/**
 * @brief Emergency inhibition of network
 *
 * WHAT: Apply global inhibition to prevent runaway excitation
 * WHY:  Last resort to prevent excitotoxic damage
 * HOW:  Scale all inhibitory synapses up, excitatory down
 *
 * EMERGENCY RESPONSE:
 * - Increase inhibitory weights by 50%
 * - Decrease excitatory weights by 25%
 * - Clamp neuromodulators to safe levels
 *
 * @param network Neural network to inhibit
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_security_emergency_inhibit(void* network);

/**
 * @brief Apply graduated inhibition increase
 *
 * WHAT: Gradually increase global inhibition to control activity
 * WHY:  Soft response to elevated activity (before emergency)
 * HOW:  Scale inhibitory synapses by factor
 *
 * @param network Neural network
 * @param scale_factor Scaling factor for inhibitory weights (e.g., 1.2 = +20%)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_security_increase_inhibition(void* network, float scale_factor);

//=============================================================================
// Security Audit and Logging
//=============================================================================

/**
 * @brief Security event type
 */
typedef enum {
    NIMCP_SECURITY_EVENT_DIRECTIVE_VERIFIED,
    NIMCP_SECURITY_EVENT_DIRECTIVE_TAMPERED,
    NIMCP_SECURITY_EVENT_INPUT_REJECTED,
    NIMCP_SECURITY_EVENT_THREAT_DETECTED,
    NIMCP_SECURITY_EVENT_ENCRYPTION_FAILED,
    NIMCP_SECURITY_EVENT_SKEPTICISM_TRIGGERED
} nimcp_security_event_t;

/**
 * @brief Log security event
 *
 * @param event Event type
 * @param severity Severity level
 * @param details Event details
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_security_log_event(nimcp_security_event_t event,
                                        nimcp_threat_level_t severity, const char* details);

/**
 * @brief Get security statistics
 *
 * @param threats_detected Output: number of threats detected
 * @param inputs_rejected Output: number of inputs rejected
 * @param directives_verified Output: number of directive verifications
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_security_get_stats(uint64_t* threats_detected, uint64_t* inputs_rejected,
                                        uint64_t* directives_verified);

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_SECURITY_H
