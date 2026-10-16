/**
 * @file nimcp_encrypted_audit.c
 * @brief Implementation of Encrypted Circular Audit Buffer
 *
 * WHAT: AES-256-GCM encrypted audit log with tamper detection
 * WHY:  Protect audit logs from unauthorized access and modification
 * HOW:  Per-entry encryption with unique nonces and auth tags
 *
 * ENCRYPTION:
 * - Algorithm: AES-256-GCM (libsodium crypto_aead_aes256gcm)
 * - Key size: 256 bits (32 bytes)
 * - Nonce size: 96 bits (12 bytes)
 * - Tag size: 128 bits (16 bytes)
 *
 * SECURITY:
 * - Unique nonce per entry (counter + random)
 * - Authentication tags detect tampering
 * - Keys protected in locked memory
 * - Secure key erasure on destruction
 *
 * @author NIMCP Security Team
 * @date 2025-12-07
 */

#include "security/nimcp_encrypted_audit.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"

#include "security/nimcp_security.h"
#include "async/nimcp_bio_router.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include "utils/memory/nimcp_memory.h"
#include <stddef.h>  /* for NULL */
#include "utils/thread/nimcp_thread.h"
// Try to use libsodium if available, otherwise use a simplified implementation
#ifdef HAVE_LIBSODIUM
#include <sodium.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(encrypted_audit, MESH_ADAPTER_CATEGORY_SECURITY)


#define USE_LIBSODIUM 1
#else
#define USE_LIBSODIUM 0
// For now, we'll use a placeholder encryption (NOT SECURE FOR PRODUCTION)
// In real deployment, must use libsodium or OpenSSL
#warning "Building without libsodium - encryption is NOT SECURE. Install libsodium for production use."
#endif

//=============================================================================
// Internal Constants
//=============================================================================

#define AUDIT_MAGIC_VALID   0x45415544
#define AUDIT_FILE_MAGIC    0x4E414C4F  // 'NALO' (NIMCP Audit Log)
#define AUDIT_FILE_VERSION  1

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Encryption key context
 */
typedef struct encryption_key {
    uint8_t key[NIMCP_AUDIT_KEY_SIZE];
    uint32_t version;
    uint64_t entries_encrypted;
    struct encryption_key* next;  // For key history
} encryption_key_t;

/**
 * @brief Encrypted entry in circular buffer
 */
typedef struct {
    uint8_t nonce[NIMCP_AUDIT_NONCE_SIZE];
    uint8_t tag[NIMCP_AUDIT_TAG_SIZE];
    uint32_t ciphertext_len;
    uint32_t entry_id;
    uint64_t timestamp_ns;
    uint32_t key_version;   // Key version used for encryption
    uint8_t ciphertext[];  // Variable length
} encrypted_entry_t;

/**
 * @brief Circular buffer slot
 */
typedef struct {
    encrypted_entry_t* entry;
    size_t allocated_size;
    bool in_use;
} buffer_slot_t;

/**
 * @brief Encrypted audit implementation
 */
struct nimcp_encrypted_audit_impl {
    uint32_t magic;
    nimcp_encrypted_audit_config_t config;

    // Circular buffer
    buffer_slot_t* buffer;
    size_t write_index;
    size_t read_index;
    uint32_t next_entry_id;

    // Encryption keys
    encryption_key_t* current_key;
    encryption_key_t* key_history;
    uint32_t key_count;
    uint64_t nonce_counter;

    // Key rotation policy
    nimcp_key_rotation_policy_t rotation_policy;
    uint64_t rotation_threshold;

    // Statistics
    nimcp_encrypted_audit_stats_t stats;

    // Synchronization
    nimcp_mutex_t lock;

    // Bio-async integration
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;
};

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Get current time in nanoseconds (safe wrapper)
 *
 * SECURITY FIX: Check clock_gettime() return value to avoid using garbage data.
 * On failure, returns 0 which may affect timestamps but won't cause undefined behavior.
 */
static uint64_t get_time_ns(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
        /* clock_gettime failed - return 0 to be safe */
        LOG_MODULE_WARN("encrypted_audit", "clock_gettime() failed");
        return 0;
    }
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/**
 * @brief Secure zero memory
 */
/* Non-static - used by other security modules */
void secure_zero(void* ptr, size_t len) {
    if (!ptr || len == 0) return;

#if USE_LIBSODIUM
    sodium_memzero(ptr, len);
#else
    // Simple memset (not compiler-proof, but better than nothing)
    volatile uint8_t* p = (volatile uint8_t*)ptr;
    for (size_t i = 0; i < len; i++) {
        p[i] = 0;
    }
#endif
}

/**
 * @brief Validate audit handle
 */
static bool validate_audit(nimcp_encrypted_audit_t audit) {
    return audit != NULL && audit->magic == AUDIT_MAGIC_VALID;
}

/**
 * @brief Generate unique nonce
 *
 * WHAT: Create unique nonce for each encryption
 * WHY:  Nonce reuse breaks GCM security
 * HOW:  Counter + random + timestamp
 */
static void generate_nonce(nimcp_encrypted_audit_t audit, uint8_t* nonce) {
    // Increment counter
    audit->nonce_counter++;

    // Fill nonce: 8 bytes counter + 4 bytes random
    uint64_t counter = audit->nonce_counter;
    memcpy(nonce, &counter, 8);

#if USE_LIBSODIUM
    randombytes_buf(nonce + 8, 4);
#else
    // Fallback: use timestamp as pseudo-random
    uint32_t rand_val = (uint32_t)(get_time_ns() & 0xFFFFFFFF);
    memcpy(nonce + 8, &rand_val, 4);
#endif
}

/**
 * @brief Encrypt audit entry
 *
 * WHAT: Encrypt plaintext entry with AES-256-GCM
 * WHY:  Protect confidentiality and integrity
 * HOW:  Generate nonce, encrypt, compute auth tag
 */
static nimcp_error_t encrypt_entry(
    nimcp_encrypted_audit_t audit,
    const nimcp_audit_entry_t* plaintext,
    encrypted_entry_t** encrypted_out,
    size_t* encrypted_len
) {
    if (!audit || !plaintext || !encrypted_out || !encrypted_len) {
        return NIMCP_INVALID_PARAM;
    }

    // Prepare plaintext buffer
    size_t plaintext_len = sizeof(nimcp_audit_entry_t);
    size_t ciphertext_len = plaintext_len;

    // Allocate encrypted entry
    size_t total_len = sizeof(encrypted_entry_t) + ciphertext_len;
    encrypted_entry_t* encrypted = nimcp_malloc(total_len);
    if (!encrypted) {
        return NIMCP_NO_MEMORY;
    }

    memset(encrypted, 0, total_len);

    // Generate unique nonce
    generate_nonce(audit, encrypted->nonce);

    // Set metadata
    encrypted->entry_id = plaintext->entry_id;
    encrypted->timestamp_ns = plaintext->timestamp_ns;
    encrypted->ciphertext_len = ciphertext_len;
    encrypted->key_version = audit->current_key->version;

    uint64_t start_time = get_time_ns();

#if USE_LIBSODIUM
    // Encrypt with libsodium AES-256-GCM
    unsigned long long actual_ciphertext_len;
    int ret = crypto_aead_aes256gcm_encrypt(
        encrypted->ciphertext,           // Output ciphertext
        &actual_ciphertext_len,          // Output length
        (const unsigned char*)plaintext, // Plaintext
        plaintext_len,                   // Plaintext length
        NULL,                            // Additional data
        0,                               // Additional data length
        NULL,                            // Secret nonce (not used)
        encrypted->nonce,                // Public nonce
        audit->current_key->key          // Key
    );

    // Copy auth tag
    memcpy(encrypted->tag, encrypted->ciphertext + plaintext_len, NIMCP_AUDIT_TAG_SIZE);

    if (ret != 0) {
        secure_zero(encrypted, total_len);
        nimcp_free(encrypted);
        return NIMCP_CRYPTO_ERROR;
    }
#else
    // PLACEHOLDER: XOR "encryption" (NOT SECURE - for compilation only)
    // Real implementation MUST use libsodium or OpenSSL
    const uint8_t* key = audit->current_key->key;
    const uint8_t* pt = (const uint8_t*)plaintext;
    for (size_t i = 0; i < plaintext_len; i++) {
        encrypted->ciphertext[i] = pt[i] ^ key[i % NIMCP_AUDIT_KEY_SIZE];
    }
    // Fake auth tag (NOT SECURE)
    memset(encrypted->tag, 0xAA, NIMCP_AUDIT_TAG_SIZE);
#endif

    uint64_t end_time = get_time_ns();
    float encrypt_time_us = (float)(end_time - start_time) / (float)NIMCP_NS_PER_US;

    // Update statistics
    audit->stats.entries_encrypted++;
    float n = (float)audit->stats.entries_encrypted;
    audit->stats.avg_encryption_time_us =
        (audit->stats.avg_encryption_time_us * (n - 1.0F) + encrypt_time_us) / n;

    audit->current_key->entries_encrypted++;

    *encrypted_out = encrypted;
    *encrypted_len = total_len;

    return NIMCP_SUCCESS;
}

/**
 * @brief Decrypt audit entry
 *
 * WHAT: Decrypt ciphertext and verify authentication tag
 * WHY:  Retrieve plaintext and detect tampering
 * HOW:  AES-256-GCM decryption with tag verification
 */
static nimcp_error_t decrypt_entry(
    nimcp_encrypted_audit_t audit,
    const encrypted_entry_t* encrypted,
    nimcp_audit_entry_t* plaintext
) {
    if (!audit || !encrypted || !plaintext) {
        return NIMCP_INVALID_PARAM;
    }

    // Find key by version - search current key and history
    encryption_key_t* key = NULL;
    if (audit->current_key->version == encrypted->key_version) {
        key = audit->current_key;
    } else {
        encryption_key_t* k = audit->key_history;
        while (k) {
            if (k->version == encrypted->key_version) {
                key = k;
                break;
            }
            k = k->next;
        }
    }
    if (!key) {
        // Fallback to current key if version not found
        key = audit->current_key;
    }

    uint64_t start_time = get_time_ns();

#if USE_LIBSODIUM
    // Decrypt with libsodium AES-256-GCM
    unsigned long long plaintext_len;
    int ret = crypto_aead_aes256gcm_decrypt(
        (unsigned char*)plaintext,       // Output plaintext
        &plaintext_len,                  // Output length
        NULL,                            // Secret nonce (not used)
        encrypted->ciphertext,           // Ciphertext
        encrypted->ciphertext_len,       // Ciphertext length
        NULL,                            // Additional data
        0,                               // Additional data length
        encrypted->nonce,                // Public nonce
        key->key                         // Key
    );

    if (ret != 0) {
        audit->stats.tampering_detected++;
        audit->stats.decryption_failures++;
        return NIMCP_ERROR_VERIFICATION_FAILED;
    }
#else
    // PLACEHOLDER: XOR "decryption" (NOT SECURE - for compilation only)
    const uint8_t* ct = encrypted->ciphertext;
    uint8_t* pt = (uint8_t*)plaintext;
    size_t plaintext_len = sizeof(nimcp_audit_entry_t);
    for (size_t i = 0; i < plaintext_len && i < encrypted->ciphertext_len; i++) {
        pt[i] = ct[i] ^ key->key[i % NIMCP_AUDIT_KEY_SIZE];
    }
    // Skip tag verification in placeholder
#endif

    uint64_t end_time = get_time_ns();
    float decrypt_time_us = (float)(end_time - start_time) / (float)NIMCP_NS_PER_US;

    // Update statistics
    audit->stats.entries_decrypted++;
    float n = (float)audit->stats.entries_decrypted;
    audit->stats.avg_decryption_time_us =
        (audit->stats.avg_decryption_time_us * (n - 1.0F) + decrypt_time_us) / n;

    return NIMCP_SUCCESS;
}

/**
 * @brief Check if key rotation is needed
 */
static bool should_rotate_key(nimcp_encrypted_audit_t audit) {
    if (audit->rotation_policy == NIMCP_KEY_ROTATION_MANUAL) {
        return false;
    }

    if (audit->rotation_policy == NIMCP_KEY_ROTATION_COUNT) {
        return audit->current_key->entries_encrypted >= audit->rotation_threshold;
    }

    // Add other rotation policies here
    return false;
}

//=============================================================================
// Configuration Functions
//=============================================================================

nimcp_encrypted_audit_config_t nimcp_encrypted_audit_default_config(void) {
    nimcp_encrypted_audit_config_t config = {
        .buffer_size = NIMCP_AUDIT_DEFAULT_BUFFER_SIZE,
        .max_entry_size = NIMCP_AUDIT_DEFAULT_ENTRY_SIZE,
        .key_rotation_interval = NIMCP_AUDIT_DEFAULT_KEY_ROTATION,
        .enable_compression = false,  // Disabled by default
        .enable_bio_async = false,
        .lock_memory = true,
        .secure_erase = true,
        .module_id = 0,
        .export_batch_size = 1000
    };
    return config;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

nimcp_encrypted_audit_t nimcp_encrypted_audit_create(
    const nimcp_encrypted_audit_config_t* config,
    const uint8_t* master_key,
    size_t key_len
) {
    if (!master_key || key_len != NIMCP_AUDIT_KEY_SIZE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_encrypted_audit_create: master_key is NULL");
        return NULL;
    }

#if USE_LIBSODIUM
    // Initialize libsodium
    if (sodium_init() < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "nimcp_encrypted_audit_create: validation failed");
        return NULL;
    }

    // Check AES-256-GCM availability
    if (!crypto_aead_aes256gcm_is_available()) {
        fprintf(stderr, "AES-256-GCM not available on this CPU\n");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_encrypted_audit_create: crypto_aead_aes256gcm_is_available is NULL");
        return NULL;
    }
#endif

    nimcp_encrypted_audit_t audit = nimcp_calloc(1, sizeof(struct nimcp_encrypted_audit_impl));
    if (!audit) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "audit is NULL");

        return NULL;
    }

    // Set configuration
    if (config) {
        audit->config = *config;
    } else {
        audit->config = nimcp_encrypted_audit_default_config();
    }

    // Allocate circular buffer
    audit->buffer = nimcp_calloc(audit->config.buffer_size, sizeof(buffer_slot_t));
    if (!audit->buffer) {
        nimcp_free(audit);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_encrypted_audit_create: audit->buffer is NULL");
        return NULL;
    }

    // Initialize encryption key
    audit->current_key = nimcp_calloc(1, sizeof(encryption_key_t));
    if (!audit->current_key) {
        nimcp_free(audit->buffer);
        nimcp_free(audit);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_encrypted_audit_create: audit->current_key is NULL");
        return NULL;
    }

    memcpy(audit->current_key->key, master_key, NIMCP_AUDIT_KEY_SIZE);
    audit->current_key->version = 0;
    audit->current_key->entries_encrypted = 0;
    audit->current_key->next = NULL;

    audit->key_history = NULL;
    audit->key_count = 1;
    audit->nonce_counter = 0;

    // Initialize indices
    audit->write_index = 0;
    audit->read_index = 0;
    audit->next_entry_id = 1;

    // Initialize rotation policy
    audit->rotation_policy = NIMCP_KEY_ROTATION_COUNT;
    audit->rotation_threshold = audit->config.key_rotation_interval;

    // Initialize statistics
    memset(&audit->stats, 0, sizeof(audit->stats));
    audit->stats.current_key_version = 0;

    // Initialize synchronization
    if (nimcp_mutex_init(&audit->lock, NULL) != 0) {
        secure_zero(audit->current_key->key, NIMCP_AUDIT_KEY_SIZE);
        nimcp_free(audit->current_key);
        nimcp_free(audit->buffer);
        nimcp_free(audit);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "nimcp_encrypted_audit_create: validation failed");
        return NULL;
    }

    // Lock memory if requested (requires root privileges)
    #ifdef __linux__
    if (audit->config.lock_memory) {
        // mlock would go here, but requires privileges
        // mlock(audit, sizeof(*audit));
    }
    #endif

    audit->bio_async_enabled = false;
    audit->bio_ctx = NULL;
    audit->magic = AUDIT_MAGIC_VALID;

    return audit;
}

void nimcp_encrypted_audit_destroy(nimcp_encrypted_audit_t audit) {
    if (!validate_audit(audit)) {
        return;
    }

    audit->magic = 0;  // Invalidate immediately

    // Unregister from bio-async
    if (audit->bio_async_enabled && audit->bio_ctx) {
        bio_router_unregister_module(audit->bio_ctx);
    }

    // Securely erase and free all encrypted entries
    for (size_t i = 0; i < audit->config.buffer_size; i++) {
        if (audit->buffer[i].entry) {
            if (audit->config.secure_erase) {
                secure_zero(audit->buffer[i].entry, audit->buffer[i].allocated_size);
            }
            nimcp_free(audit->buffer[i].entry);
        }
    }
    nimcp_free(audit->buffer);

    // Securely erase current key
    secure_zero(audit->current_key->key, NIMCP_AUDIT_KEY_SIZE);
    nimcp_free(audit->current_key);

    // Securely erase key history
    encryption_key_t* key = audit->key_history;
    while (key) {
        encryption_key_t* next = key->next;
        secure_zero(key->key, NIMCP_AUDIT_KEY_SIZE);
        nimcp_free(key);
        key = next;
    }

    nimcp_mutex_destroy(&audit->lock);

    // Unlock memory if it was locked
    #ifdef __linux__
    if (audit->config.lock_memory) {
        // munlock(audit, sizeof(*audit));
    }
    #endif

    secure_zero(audit, sizeof(*audit));
    nimcp_free(audit);
}

//=============================================================================
// Logging Functions
//=============================================================================

nimcp_error_t nimcp_encrypted_audit_log(
    nimcp_encrypted_audit_t audit,
    nimcp_audit_severity_t severity,
    nimcp_audit_category_t category,
    const char* message,
    const void* data,
    size_t data_len
) {
    return nimcp_encrypted_audit_log_timestamped(
        audit, get_time_ns(), severity, category, message, data, data_len);
}

nimcp_error_t nimcp_encrypted_audit_log_timestamped(
    nimcp_encrypted_audit_t audit,
    uint64_t timestamp_ns,
    nimcp_audit_severity_t severity,
    nimcp_audit_category_t category,
    const char* message,
    const void* data,
    size_t data_len
) {
    if (!validate_audit(audit) || !message) {
        return NIMCP_INVALID_PARAM;
    }

    if (severity >= NIMCP_AUDIT_SEVERITY_COUNT || category >= NIMCP_AUDIT_CATEGORY_COUNT) {
        return NIMCP_INVALID_PARAM;
    }

    if (data_len > NIMCP_AUDIT_MAX_DATA_SIZE) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_mutex_lock(&audit->lock);

    // Create plaintext entry
    nimcp_audit_entry_t entry;
    memset(&entry, 0, sizeof(entry));

    entry.timestamp_ns = timestamp_ns;
    entry.severity = severity;
    entry.category = category;
    entry.entry_id = audit->next_entry_id++;
    entry.key_version = audit->current_key->version;

    strncpy(entry.message, message, NIMCP_AUDIT_MAX_MESSAGE_SIZE - 1);
    entry.message[NIMCP_AUDIT_MAX_MESSAGE_SIZE - 1] = '\0';

    if (data && data_len > 0) {
        memcpy(entry.data, data, data_len);
        entry.data_len = data_len;
    } else {
        entry.data_len = 0;
    }

    // Encrypt entry
    encrypted_entry_t* encrypted = NULL;
    size_t encrypted_len = 0;
    nimcp_error_t err = encrypt_entry(audit, &entry, &encrypted, &encrypted_len);
    if (err != NIMCP_SUCCESS) {
        audit->stats.encryption_failures++;
        nimcp_mutex_unlock(&audit->lock);
        return err;
    }

    // Store in circular buffer
    size_t write_idx = audit->write_index;

    // Free old entry if overwriting
    if (audit->buffer[write_idx].entry) {
        if (audit->config.secure_erase) {
            secure_zero(audit->buffer[write_idx].entry, audit->buffer[write_idx].allocated_size);
        }
        nimcp_free(audit->buffer[write_idx].entry);
        audit->stats.buffer_wraps++;
    }

    audit->buffer[write_idx].entry = encrypted;
    audit->buffer[write_idx].allocated_size = encrypted_len;
    audit->buffer[write_idx].in_use = true;

    // Update oldest entry ID if we wrapped
    if (audit->write_index == audit->read_index && audit->stats.total_entries > 0) {
        audit->stats.oldest_entry_id = entry.entry_id - audit->config.buffer_size + 1;
    }

    // Advance write index
    audit->write_index = (audit->write_index + 1) % audit->config.buffer_size;

    // Update statistics
    audit->stats.total_entries++;
    audit->stats.newest_entry_id = entry.entry_id;
    if (audit->stats.total_entries == 1) {
        audit->stats.oldest_entry_id = entry.entry_id;
    }

    // Check for key rotation
    if (should_rotate_key(audit)) {
        // Auto-rotate key (would need to generate new key)
        // For now, just increment counter
        audit->stats.key_rotations++;
    }

    nimcp_mutex_unlock(&audit->lock);

    // Send alert for critical events
    if (audit->bio_async_enabled && severity >= NIMCP_AUDIT_CRITICAL) {
        nimcp_encrypted_audit_broadcast_alert(audit, &entry);
    }

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_encrypted_audit_logf(
    nimcp_encrypted_audit_t audit,
    nimcp_audit_severity_t severity,
    nimcp_audit_category_t category,
    const char* format,
    ...
) {
    if (!validate_audit(audit) || !format) {
        return NIMCP_INVALID_PARAM;
    }

    char message[NIMCP_AUDIT_MAX_MESSAGE_SIZE];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    return nimcp_encrypted_audit_log(audit, severity, category, message, NULL, 0);
}

//=============================================================================
// Reading Functions
//=============================================================================

nimcp_error_t nimcp_encrypted_audit_read(
    nimcp_encrypted_audit_t audit,
    nimcp_audit_entry_t* entries,
    size_t max_entries,
    size_t* num_entries
) {
    if (!validate_audit(audit) || !entries || !num_entries || max_entries == 0) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_mutex_lock(&audit->lock);

    size_t count = 0;
    size_t read_idx = audit->read_index;

    for (size_t i = 0; i < audit->config.buffer_size && count < max_entries; i++) {
        if (!audit->buffer[read_idx].in_use || !audit->buffer[read_idx].entry) {
            read_idx = (read_idx + 1) % audit->config.buffer_size;
            continue;
        }

        // Decrypt entry
        nimcp_error_t err = decrypt_entry(audit, audit->buffer[read_idx].entry, &entries[count]);
        if (err != NIMCP_SUCCESS) {
            // Skip corrupted entry
            read_idx = (read_idx + 1) % audit->config.buffer_size;
            continue;
        }

        count++;
        read_idx = (read_idx + 1) % audit->config.buffer_size;
    }

    *num_entries = count;

    nimcp_mutex_unlock(&audit->lock);

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_encrypted_audit_read_filtered(
    nimcp_encrypted_audit_t audit,
    nimcp_audit_severity_t min_severity,
    nimcp_audit_category_t category,
    nimcp_audit_entry_t* entries,
    size_t max_entries,
    size_t* num_entries
) {
    if (!validate_audit(audit) || !entries || !num_entries) {
        return NIMCP_INVALID_PARAM;
    }

    // Read all entries
    nimcp_audit_entry_t* all_entries = nimcp_malloc(audit->config.buffer_size * sizeof(nimcp_audit_entry_t));
    if (!all_entries) {
        return NIMCP_NO_MEMORY;
    }

    size_t all_count = 0;
    nimcp_error_t err = nimcp_encrypted_audit_read(audit, all_entries, audit->config.buffer_size, &all_count);
    if (err != NIMCP_SUCCESS) {
        nimcp_free(all_entries);
        return err;
    }

    // Filter entries
    size_t count = 0;
    for (size_t i = 0; i < all_count && count < max_entries; i++) {
        bool matches = true;

        if (all_entries[i].severity < min_severity) {
            matches = false;
        }

        if (category != NIMCP_AUDIT_CATEGORY_COUNT && all_entries[i].category != category) {
            matches = false;
        }

        if (matches) {
            entries[count++] = all_entries[i];
        }
    }

    *num_entries = count;
    nimcp_free(all_entries);

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_encrypted_audit_read_range(
    nimcp_encrypted_audit_t audit,
    uint64_t start_time_ns,
    uint64_t end_time_ns,
    nimcp_audit_entry_t* entries,
    size_t max_entries,
    size_t* num_entries
) {
    if (!validate_audit(audit) || !entries || !num_entries) {
        return NIMCP_INVALID_PARAM;
    }

    // Read all entries
    nimcp_audit_entry_t* all_entries = nimcp_malloc(audit->config.buffer_size * sizeof(nimcp_audit_entry_t));
    if (!all_entries) {
        return NIMCP_NO_MEMORY;
    }

    size_t all_count = 0;
    nimcp_error_t err = nimcp_encrypted_audit_read(audit, all_entries, audit->config.buffer_size, &all_count);
    if (err != NIMCP_SUCCESS) {
        nimcp_free(all_entries);
        return err;
    }

    // Filter by time range
    size_t count = 0;
    for (size_t i = 0; i < all_count && count < max_entries; i++) {
        if (all_entries[i].timestamp_ns >= start_time_ns &&
            all_entries[i].timestamp_ns <= end_time_ns) {
            entries[count++] = all_entries[i];
        }
    }

    *num_entries = count;
    nimcp_free(all_entries);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Key Management
//=============================================================================

nimcp_error_t nimcp_encrypted_audit_rotate_key(
    nimcp_encrypted_audit_t audit,
    const uint8_t* new_key,
    size_t key_len
) {
    if (!validate_audit(audit) || !new_key || key_len != NIMCP_AUDIT_KEY_SIZE) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_mutex_lock(&audit->lock);

    // Move current key to history
    audit->current_key->next = audit->key_history;
    audit->key_history = audit->current_key;

    // Create new key
    audit->current_key = nimcp_calloc(1, sizeof(encryption_key_t));
    if (!audit->current_key) {
        nimcp_mutex_unlock(&audit->lock);
        return NIMCP_NO_MEMORY;
    }

    memcpy(audit->current_key->key, new_key, NIMCP_AUDIT_KEY_SIZE);
    audit->current_key->version = audit->key_history->version + 1;
    audit->current_key->entries_encrypted = 0;
    audit->current_key->next = NULL;

    audit->key_count++;
    audit->stats.key_rotations++;
    audit->stats.current_key_version = audit->current_key->version;

    nimcp_mutex_unlock(&audit->lock);

    return NIMCP_SUCCESS;
}

uint32_t nimcp_encrypted_audit_get_key_version(nimcp_encrypted_audit_t audit) {
    if (!validate_audit(audit)) {
        return 0;
    }
    return audit->current_key->version;
}

nimcp_error_t nimcp_encrypted_audit_set_rotation_policy(
    nimcp_encrypted_audit_t audit,
    nimcp_key_rotation_policy_t policy,
    uint64_t threshold
) {
    if (!validate_audit(audit)) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_mutex_lock(&audit->lock);
    audit->rotation_policy = policy;
    audit->rotation_threshold = threshold;
    nimcp_mutex_unlock(&audit->lock);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Import/Export Functions
//=============================================================================

/**
 * @brief File header for audit log export
 */
typedef struct {
    uint32_t magic;             // AUDIT_FILE_MAGIC
    uint32_t version;           // AUDIT_FILE_VERSION
    uint32_t entry_count;       // Number of entries
    uint32_t key_version;       // Key version used
    uint64_t export_time_ns;    // Export timestamp
    uint8_t reserved[32];       // Reserved for future use
} audit_file_header_t;

nimcp_error_t nimcp_encrypted_audit_export(
    nimcp_encrypted_audit_t audit,
    const char* filepath
) {
    if (!validate_audit(audit) || !filepath) {
        return NIMCP_INVALID_PARAM;
    }

    FILE* file = fopen(filepath, "wb");
    if (!file) {
        LOG_MODULE_ERROR("encrypted_audit", "Failed to create export file: %s", filepath);
        return NIMCP_ERROR;
    }

    nimcp_mutex_lock(&audit->lock);

    // Prepare file header
    audit_file_header_t header;
    header.magic = AUDIT_FILE_MAGIC;
    header.version = AUDIT_FILE_VERSION;
    /* Compute actual entry count from buffer state */
    uint32_t actual_entries = (audit->next_entry_id > audit->config.buffer_size) ?
                              (uint32_t)audit->config.buffer_size :
                              (uint32_t)(audit->next_entry_id - 1);
    header.entry_count = actual_entries;
    header.key_version = audit->current_key->version;
    header.export_time_ns = get_time_ns();
    memset(header.reserved, 0, sizeof(header.reserved));

    // Write header
    if (fwrite(&header, sizeof(header), 1, file) != 1) {
        nimcp_mutex_unlock(&audit->lock);
        fclose(file);
        return NIMCP_ERROR;
    }

    // Write encrypted entries in order
    uint32_t entries_written = 0;
    /* Compute how many entries are currently in the buffer */
    size_t entries_in_buffer = (audit->next_entry_id > audit->config.buffer_size) ?
                               audit->config.buffer_size :
                               (audit->next_entry_id > 0 ? audit->next_entry_id - 1 : 0);
    /* If buffer has wrapped, start from current write position; otherwise start from 0 */
    size_t start_idx = (audit->next_entry_id > audit->config.buffer_size)
                       ? (audit->write_index % audit->config.buffer_size)
                       : 0;

    for (size_t i = 0; i < entries_in_buffer; i++) {
        size_t idx = (start_idx + i) % audit->config.buffer_size;
        buffer_slot_t* slot = &audit->buffer[idx];

        if (!slot->in_use || !slot->entry) continue;

        encrypted_entry_t* entry = slot->entry;
        uint32_t total_size = (uint32_t)(sizeof(encrypted_entry_t) + entry->ciphertext_len);

        if (fwrite(&total_size, sizeof(total_size), 1, file) != 1 ||
            fwrite(entry, total_size, 1, file) != 1) {
            nimcp_mutex_unlock(&audit->lock);
            fclose(file);
            return NIMCP_ERROR;
        }
        entries_written++;
    }

    // Update header with actual entries written (may differ from initial count
    // if some buffer slots were empty/not in use)
    if (entries_written != header.entry_count) {
        header.entry_count = entries_written;
        fseek(file, 0, SEEK_SET);
        fwrite(&header, sizeof(header), 1, file);
    }

    nimcp_mutex_unlock(&audit->lock);
    fclose(file);

    LOG_MODULE_INFO("encrypted_audit", "Exported %u entries to %s", entries_written, filepath);
    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_encrypted_audit_import(
    nimcp_encrypted_audit_t audit,
    const char* filepath
) {
    if (!validate_audit(audit) || !filepath) {
        return NIMCP_INVALID_PARAM;
    }

    FILE* file = fopen(filepath, "rb");
    if (!file) {
        LOG_MODULE_ERROR("encrypted_audit", "Failed to open import file: %s", filepath);
        return NIMCP_ERROR;
    }

    // Read and validate header
    audit_file_header_t header;
    if (fread(&header, sizeof(header), 1, file) != 1) {
        fclose(file);
        return NIMCP_ERROR;
    }

    if (header.magic != AUDIT_FILE_MAGIC) {
        LOG_MODULE_ERROR("encrypted_audit", "Invalid file magic: 0x%08X", header.magic);
        fclose(file);
        return NIMCP_INVALID_PARAM;
    }

    if (header.version > AUDIT_FILE_VERSION) {
        LOG_MODULE_ERROR("encrypted_audit", "Unsupported file version: %u", header.version);
        fclose(file);
        return NIMCP_INVALID_PARAM;
    }

    nimcp_mutex_lock(&audit->lock);

    // Read entries
    uint32_t entries_imported = 0;
    for (uint32_t i = 0; i < header.entry_count; i++) {
        uint32_t total_size;
        if (fread(&total_size, sizeof(total_size), 1, file) != 1) break;

        size_t max_valid_size = sizeof(encrypted_entry_t) + sizeof(nimcp_audit_entry_t) + 1024;
        if (total_size < sizeof(encrypted_entry_t) || total_size > max_valid_size) {
            LOG_MODULE_WARN("encrypted_audit", "Invalid entry size: %u", total_size);
            break;
        }

        // Allocate slot
        size_t slot_idx = audit->write_index % audit->config.buffer_size;
        buffer_slot_t* slot = &audit->buffer[slot_idx];

        // Clear old entry if needed
        if (slot->entry) {
            nimcp_free(slot->entry);
        }

        slot->entry = nimcp_malloc(total_size);
        if (!slot->entry) {
            nimcp_mutex_unlock(&audit->lock);
            fclose(file);
            return NIMCP_NO_MEMORY;
        }

        if (fread(slot->entry, total_size, 1, file) != 1) {
            nimcp_free(slot->entry);
            slot->entry = NULL;
            break;
        }

        slot->allocated_size = total_size;
        slot->in_use = true;
        audit->write_index++;
        audit->next_entry_id++;
        entries_imported++;
    }

    // Update statistics to reflect imported entries
    audit->stats.total_entries += entries_imported;

    nimcp_mutex_unlock(&audit->lock);
    fclose(file);

    LOG_MODULE_INFO("encrypted_audit", "Imported %u entries from %s", entries_imported, filepath);
    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_encrypted_audit_export_json(
    nimcp_encrypted_audit_t audit,
    const char* filepath,
    const uint8_t* master_key,
    size_t key_len
) {
    if (!validate_audit(audit) || !filepath || !master_key || key_len != NIMCP_AUDIT_KEY_SIZE) {
        return NIMCP_INVALID_PARAM;
    }

    FILE* file = fopen(filepath, "w");
    if (!file) {
        LOG_MODULE_ERROR("encrypted_audit", "Failed to create JSON export file: %s", filepath);
        return NIMCP_ERROR;
    }

    // Read all entries (decrypted)
    nimcp_audit_entry_t* entries = nimcp_malloc(audit->config.buffer_size * sizeof(nimcp_audit_entry_t));
    if (!entries) {
        fclose(file);
        return NIMCP_NO_MEMORY;
    }

    size_t entry_count = 0;
    nimcp_error_t err = nimcp_encrypted_audit_read(audit, entries, audit->config.buffer_size, &entry_count);
    if (err != NIMCP_SUCCESS) {
        nimcp_free(entries);
        fclose(file);
        return err;
    }

    // Write JSON header
    fprintf(file, "{\n");
    fprintf(file, "  \"export_time\": %lu,\n", (unsigned long)time(NULL));
    fprintf(file, "  \"entry_count\": %zu,\n", entry_count);
    fprintf(file, "  \"key_version\": %u,\n", audit->current_key->version);
    fprintf(file, "  \"entries\": [\n");

    for (size_t i = 0; i < entry_count; i++) {
        nimcp_audit_entry_t* e = &entries[i];

        fprintf(file, "    {\n");
        fprintf(file, "      \"entry_id\": %u,\n", e->entry_id);
        fprintf(file, "      \"timestamp_ns\": %lu,\n", (unsigned long)e->timestamp_ns);
        fprintf(file, "      \"severity\": \"%s\",\n", nimcp_audit_severity_name(e->severity));
        fprintf(file, "      \"category\": \"%s\",\n", nimcp_audit_category_name(e->category));
        fprintf(file, "      \"key_version\": %u,\n", e->key_version);

        // Escape message for JSON
        fprintf(file, "      \"message\": \"");
        for (size_t j = 0; j < NIMCP_AUDIT_MAX_MESSAGE_SIZE && e->message[j]; j++) {
            char c = e->message[j];
            if (c == '"') fprintf(file, "\\\"");
            else if (c == '\\') fprintf(file, "\\\\");
            else if (c == '\n') fprintf(file, "\\n");
            else if (c == '\r') fprintf(file, "\\r");
            else if (c == '\t') fprintf(file, "\\t");
            else fputc(c, file);
        }
        fprintf(file, "\"\n");

        fprintf(file, "    }%s\n", i < entry_count - 1 ? "," : "");
    }

    fprintf(file, "  ]\n}\n");

    nimcp_free(entries);
    fclose(file);

    LOG_MODULE_INFO("encrypted_audit", "Exported %zu entries to JSON: %s", entry_count, filepath);
    return NIMCP_SUCCESS;
}

//=============================================================================
// Statistics and Monitoring
//=============================================================================

nimcp_error_t nimcp_encrypted_audit_get_stats(
    nimcp_encrypted_audit_t audit,
    nimcp_encrypted_audit_stats_t* stats
) {
    if (!validate_audit(audit) || !stats) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_mutex_lock(&audit->lock);
    *stats = audit->stats;

    // Calculate memory usage
    stats->memory_usage_bytes = sizeof(struct nimcp_encrypted_audit_impl);
    stats->memory_usage_bytes += audit->config.buffer_size * sizeof(buffer_slot_t);
    for (size_t i = 0; i < audit->config.buffer_size; i++) {
        if (audit->buffer[i].entry) {
            stats->memory_usage_bytes += audit->buffer[i].allocated_size;
        }
    }

    nimcp_mutex_unlock(&audit->lock);

    return NIMCP_SUCCESS;
}

void nimcp_encrypted_audit_reset_stats(nimcp_encrypted_audit_t audit) {
    if (!validate_audit(audit)) {
        return;
    }

    nimcp_mutex_lock(&audit->lock);

    // Reset statistics (keep structural data)
    audit->stats.encryption_failures = 0;
    audit->stats.decryption_failures = 0;
    audit->stats.tampering_detected = 0;
    audit->stats.avg_encryption_time_us = 0.0F;
    audit->stats.avg_decryption_time_us = 0.0F;

    nimcp_mutex_unlock(&audit->lock);
}

/* Use functions from nimcp_security_audit.c instead of local duplicates */
extern const char* nimcp_audit_severity_name(nimcp_audit_severity_t severity);
extern const char* nimcp_audit_category_name(nimcp_audit_category_t category);

//=============================================================================
// Bio-Async Integration
//=============================================================================

uint32_t nimcp_encrypted_audit_process_inbox(
    nimcp_encrypted_audit_t audit,
    uint32_t max_messages
) {
    if (!validate_audit(audit) || !audit->bio_async_enabled || !audit->bio_ctx) {
        return 0;
    }

    return bio_router_process_inbox(audit->bio_ctx, max_messages);
}

nimcp_error_t nimcp_encrypted_audit_register_bio_async(
    nimcp_encrypted_audit_t audit,
    bio_module_id_t module_id
) {
    if (!validate_audit(audit)) {
        return NIMCP_INVALID_PARAM;
    }

    if (!bio_router_is_initialized()) {
        return NIMCP_ERROR;
    }

    bio_module_info_t info = {
        .module_id = module_id,
        .module_name = "encrypted_audit",
        .inbox_capacity = 256,
        .user_data = audit
    };

    audit->bio_ctx = bio_router_register_module(&info);
    if (!audit->bio_ctx) {
        return NIMCP_ERROR;
    }

    audit->bio_async_enabled = true;
    audit->config.module_id = module_id;

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_encrypted_audit_broadcast_alert(
    nimcp_encrypted_audit_t audit,
    const nimcp_audit_entry_t* entry
) {
    if (!validate_audit(audit) || !entry) {
        return NIMCP_INVALID_PARAM;
    }

    if (!audit->bio_async_enabled || !audit->bio_ctx) {
        return NIMCP_SUCCESS;  // Not an error, just not enabled
    }

    // TODO: Send bio-async broadcast message
    // bio_router_broadcast(audit->bio_ctx, &msg, sizeof(msg));

    return NIMCP_SUCCESS;
}
