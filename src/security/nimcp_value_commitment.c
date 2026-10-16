/**
 * @file nimcp_value_commitment.c
 * @brief Cryptographic Value Commitment Implementation
 * @version 1.0.0
 * @date 2026-02-01
 */

#include "security/nimcp_value_commitment.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/thread/nimcp_thread.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utils/memory/nimcp_memory.h"

#define LOG_CATEGORY "value_commitment"

#include "utils/exception/nimcp_exception_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(value_commitment, MESH_ADAPTER_CATEGORY_SECURITY)


struct value_commitment_system {
    uint32_t magic;
    nimcp_mutex_t* mutex;
    value_commitment_config_t config;
    value_commitment_stats_t stats;

    /* Bio-async integration */
    bio_module_context_t bio_ctx;
    bool bio_async_connected;
};

static bool is_valid_handle(const value_commitment_system_t* system) {
    return system != NULL && system->magic == VALUE_COMMITMENT_MAGIC;
}

static uint64_t get_time_us(void) { return nimcp_time_now_us(); }

static void safe_strcpy(char* dest, const char* src, size_t max_len) {
    if (dest == NULL || max_len == 0) return;
    if (src == NULL) { dest[0] = '\0'; return; }
    size_t len = strlen(src);
    if (len >= max_len) len = max_len - 1;
    memcpy(dest, src, len);
    dest[len] = '\0';
}

/* Simple hash for values (placeholder - would use SHA-256 in production) */
static void compute_value_hash(
    const alignment_weights_t* values,
    uint8_t* hash_out)
{
    memset(hash_out, 0, VALUE_HASH_SIZE);

    /* Simple placeholder hash - XOR values into hash bytes */
    for (uint32_t i = 0; i < values->value_count && i < 16; i++) {
        uint32_t val_int = *(uint32_t*)&values->values[i];
        hash_out[i * 2 % VALUE_HASH_SIZE] ^= (val_int >> 24) & 0xFF;
        hash_out[(i * 2 + 1) % VALUE_HASH_SIZE] ^= (val_int >> 16) & 0xFF;
    }

    /* Add count for differentiation */
    hash_out[0] ^= values->value_count;
}

value_commitment_config_t value_commitment_default_config(void) {
    value_commitment_config_t config;
    memset(&config, 0, sizeof(config));
    config.enable_periodic_attestation = true;
    config.attestation_interval_ms = 60000;
    config.require_signature_verification = true;
    return config;
}

value_commitment_system_t* value_commitment_system_create(
    const value_commitment_config_t* config)
{
    value_commitment_system_t* system = nimcp_calloc(1, sizeof(value_commitment_system_t));
    if (system == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "value_commitment_system_create: validation failed");
        return NULL;
    }

    system->mutex = nimcp_mutex_create(NULL);
    if (system->mutex == NULL) { nimcp_free(system); return NULL; }

    if (config) memcpy(&system->config, config, sizeof(*config));
    else system->config = value_commitment_default_config();

    system->magic = VALUE_COMMITMENT_MAGIC;
    NIMCP_LOG_INFO(LOG_CATEGORY, "Value commitment system created");
    return system;
}

void value_commitment_system_destroy(value_commitment_system_t* system) {
    if (!is_valid_handle(system)) return;

    /* Unregister from bio-async */
    if (system->bio_async_connected && system->bio_ctx) {
        bio_router_unregister_module(system->bio_ctx);
        system->bio_ctx = NULL;
        system->bio_async_connected = false;
    }

    system->magic = 0;
    if (system->mutex) nimcp_mutex_destroy(system->mutex);
    nimcp_free(system);
    NIMCP_LOG_INFO(LOG_CATEGORY, "Value commitment system destroyed");
}

nimcp_error_t value_commitment_create(
    value_commitment_system_t* system,
    value_commitment_t* commitment,
    const alignment_weights_t* values,
    const char* signer_id)
{
    if (!is_valid_handle(system) || commitment == NULL || values == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT, "value_commitment: error condition");
        return NIMCP_ERROR_INVALID_ARGUMENT;
    }

    nimcp_mutex_lock(system->mutex);

    memset(commitment, 0, sizeof(*commitment));

    /* Compute merkle root (placeholder - just hash the values) */
    compute_value_hash(values, commitment->value_merkle_root);

    /* Record signer and timestamp */
    safe_strcpy(commitment->signer_identity, signer_id, VALUE_SIGNER_ID_MAX);
    commitment->initialization_timestamp = get_time_us();

    /* Create initialization signature (placeholder) */
    memcpy(commitment->initialization_signature, commitment->value_merkle_root, VALUE_HASH_SIZE);

    /* Store locked values */
    memcpy(&commitment->locked_values, values, sizeof(*values));

    system->stats.commitments_created++;

    nimcp_mutex_unlock(system->mutex);

    NIMCP_LOG_INFO(LOG_CATEGORY, "Value commitment created by '%s'",
        signer_id ? signer_id : "unknown");

    return NIMCP_OK;
}

nimcp_error_t value_commitment_verify(
    value_commitment_system_t* system,
    const value_commitment_t* commitment,
    const alignment_weights_t* current_values,
    bool* valid,
    char* tampering_report,
    size_t report_size)
{
    if (!is_valid_handle(system) || commitment == NULL ||
        current_values == NULL || valid == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT, "value_commitment: error condition");
        return NIMCP_ERROR_INVALID_ARGUMENT;
    }

    nimcp_mutex_lock(system->mutex);

    /* Compute hash of current values */
    uint8_t current_hash[VALUE_HASH_SIZE];
    compute_value_hash(current_values, current_hash);

    /* Compare to committed hash */
    *valid = (memcmp(current_hash, commitment->value_merkle_root, VALUE_HASH_SIZE) == 0);

    if (!*valid && tampering_report && report_size > 0) {
        /* Find which values changed */
        for (uint32_t i = 0; i < current_values->value_count; i++) {
            if (i < commitment->locked_values.value_count &&
                current_values->values[i] != commitment->locked_values.values[i]) {
                snprintf(tampering_report, report_size,
                    "Value %u changed from %.4f to %.4f",
                    i, commitment->locked_values.values[i], current_values->values[i]);
                break;
            }
        }
        system->stats.tampering_detected++;
    }

    system->stats.verifications_performed++;

    nimcp_mutex_unlock(system->mutex);

    NIMCP_LOG_DEBUG(LOG_CATEGORY, "Value commitment verification: %s",
        *valid ? "VALID" : "TAMPERED");

    return NIMCP_OK;
}

nimcp_error_t value_commitment_attest(
    value_commitment_system_t* system,
    value_commitment_t* commitment,
    const alignment_weights_t* current_values,
    attestation_t* attestation)
{
    if (!is_valid_handle(system) || commitment == NULL ||
        current_values == NULL || attestation == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT, "value_commitment: error condition");
        return NIMCP_ERROR_INVALID_ARGUMENT;
    }

    nimcp_mutex_lock(system->mutex);

    memset(attestation, 0, sizeof(*attestation));
    attestation->timestamp = get_time_us();

    /* Compute hash of current values */
    compute_value_hash(current_values, attestation->value_hash);

    /* Create signature (placeholder) */
    memcpy(attestation->signature, attestation->value_hash, VALUE_HASH_SIZE);

    /* Copy signer ID */
    safe_strcpy(attestation->signer_id, commitment->signer_identity, VALUE_SIGNER_ID_MAX);

    /* Add to attestation chain */
    if (commitment->attestation_count < VALUE_ATTESTATION_CHAIN_MAX) {
        memcpy(commitment->attestation_chain[commitment->attestation_count],
               attestation->value_hash, VALUE_HASH_SIZE);
        commitment->attestation_count++;
    }
    commitment->last_attestation_timestamp = attestation->timestamp;

    system->stats.attestations_generated++;

    nimcp_mutex_unlock(system->mutex);

    NIMCP_LOG_DEBUG(LOG_CATEGORY, "Attestation generated (chain length: %u)",
        commitment->attestation_count);

    return NIMCP_OK;
}

nimcp_error_t value_commitment_verify_attestation(
    value_commitment_system_t* system,
    const attestation_t* attestation,
    bool* valid)
{
    if (!is_valid_handle(system) || attestation == NULL || valid == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT, "value_commitment: error condition");
        return NIMCP_ERROR_INVALID_ARGUMENT;
    }

    nimcp_mutex_lock(system->mutex);

    /* Verify signature matches hash (placeholder verification) */
    *valid = (memcmp(attestation->signature, attestation->value_hash, VALUE_HASH_SIZE) == 0);

    nimcp_mutex_unlock(system->mutex);

    return NIMCP_OK;
}

nimcp_error_t value_commitment_get_stats(
    const value_commitment_system_t* system,
    value_commitment_stats_t* stats)
{
    if (!is_valid_handle(system) || stats == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT, "value_commitment: error condition");
        return NIMCP_ERROR_INVALID_ARGUMENT;
    }

    value_commitment_system_t* mutable_system = (value_commitment_system_t*)system;
    nimcp_mutex_lock(mutable_system->mutex);
    memcpy(stats, &system->stats, sizeof(*stats));
    nimcp_mutex_unlock(mutable_system->mutex);

    return NIMCP_OK;
}

nimcp_error_t value_commitment_connect_bio_async(value_commitment_system_t* system) {
    if (!is_valid_handle(system)) return NIMCP_ERROR_INVALID_ARGUMENT;

    nimcp_mutex_lock(system->mutex);

    if (system->bio_async_connected) {
        nimcp_mutex_unlock(system->mutex);
        return NIMCP_OK;
    }

    bio_module_info_t module_info = {
        .module_id = BIO_MODULE_VALUE_COMMITMENT,
        .module_name = "value_commitment",
        .inbox_capacity = 0,
        .user_data = system
    };

    system->bio_ctx = bio_router_register_module(&module_info);
    if (!system->bio_ctx) {
        NIMCP_LOG_WARN(LOG_CATEGORY, "Bio-async registration failed - continuing without");
        nimcp_mutex_unlock(system->mutex);
        return NIMCP_OK;
    }

    system->bio_async_connected = true;
    nimcp_mutex_unlock(system->mutex);

    NIMCP_LOG_INFO(LOG_CATEGORY, "Connected to bio-async messaging");
    return NIMCP_OK;
}
