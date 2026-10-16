/**
 * @file nimcp_value_commitment.h
 * @brief Cryptographic Value Commitment for AI Safety
 * @version 1.0.0
 * @date 2026-02-01
 *
 * WHAT: Cryptographic commitment to initial value configuration
 * WHY:  Tamper-evident record of value alignment at initialization
 * HOW:  Merkle root, periodic attestation chain, signature verification
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_VALUE_COMMITMENT_H
#define NIMCP_VALUE_COMMITMENT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "utils/error/nimcp_error_codes.h"
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VALUE_COMMITMENT_MAGIC          0x56414C43  /* "VALC" */
#define VALUE_HASH_SIZE                 32
#define VALUE_SIGNATURE_SIZE            64
#define VALUE_SIGNER_ID_MAX             128
#define VALUE_ATTESTATION_CHAIN_MAX     256
#define VALUE_REPORT_MAX                1024

typedef struct alignment_weights {
    float values[16];
    uint32_t value_count;
} alignment_weights_t;

typedef struct attestation {
    uint64_t timestamp;
    uint8_t value_hash[VALUE_HASH_SIZE];
    uint8_t signature[VALUE_SIGNATURE_SIZE];
    char signer_id[VALUE_SIGNER_ID_MAX];
} attestation_t;

typedef struct value_commitment {
    uint8_t value_merkle_root[VALUE_HASH_SIZE];
    uint8_t initialization_signature[VALUE_SIGNATURE_SIZE];
    char signer_identity[VALUE_SIGNER_ID_MAX];
    uint64_t initialization_timestamp;
    uint64_t last_attestation_timestamp;
    uint8_t attestation_chain[VALUE_ATTESTATION_CHAIN_MAX][VALUE_HASH_SIZE];
    uint32_t attestation_count;
    alignment_weights_t locked_values;
} value_commitment_t;

typedef struct value_commitment_config {
    bool enable_periodic_attestation;
    uint32_t attestation_interval_ms;
    bool require_signature_verification;
} value_commitment_config_t;

typedef struct value_commitment_stats {
    uint64_t commitments_created;
    uint64_t verifications_performed;
    uint64_t attestations_generated;
    uint64_t tampering_detected;
} value_commitment_stats_t;

typedef struct value_commitment_system value_commitment_system_t;

NIMCP_EXPORT value_commitment_config_t value_commitment_default_config(void);
NIMCP_EXPORT value_commitment_system_t* value_commitment_system_create(
    const value_commitment_config_t* config
);
NIMCP_EXPORT void value_commitment_system_destroy(value_commitment_system_t* system);

NIMCP_EXPORT nimcp_error_t value_commitment_create(
    value_commitment_system_t* system,
    value_commitment_t* commitment,
    const alignment_weights_t* values,
    const char* signer_id
);

NIMCP_EXPORT nimcp_error_t value_commitment_verify(
    value_commitment_system_t* system,
    const value_commitment_t* commitment,
    const alignment_weights_t* current_values,
    bool* valid,
    char* tampering_report,
    size_t report_size
);

NIMCP_EXPORT nimcp_error_t value_commitment_attest(
    value_commitment_system_t* system,
    value_commitment_t* commitment,
    const alignment_weights_t* current_values,
    attestation_t* attestation
);

NIMCP_EXPORT nimcp_error_t value_commitment_verify_attestation(
    value_commitment_system_t* system,
    const attestation_t* attestation,
    bool* valid
);

NIMCP_EXPORT nimcp_error_t value_commitment_get_stats(
    const value_commitment_system_t* system,
    value_commitment_stats_t* stats
);

NIMCP_EXPORT nimcp_error_t value_commitment_connect_bio_async(
    value_commitment_system_t* system
);

/* ============================================================================
 * Health Agent Integration
 * ============================================================================ */

/**
 * @brief Set health agent for heartbeat reporting
 *
 * @param agent Health agent handle from brain init
 */
struct nimcp_health_agent;
NIMCP_EXPORT void value_commitment_set_health_agent(struct nimcp_health_agent* agent);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_VALUE_COMMITMENT_H */
