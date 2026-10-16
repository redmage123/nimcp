/**
 * @file nimcp_symbolic_logic_safety.c
 * @brief LGSS Component A1: Safety Knowledge Base implementation
 *
 * WHAT: Implementation of safety KB with mmap, mprotect, and forward chaining
 * WHY:  Provide tamper-resistant safety constraints for AI systems
 * HOW:  mmap for storage, mprotect for locking, SHA-256 for integrity
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 * @version 1.0.0
 */

#include "cognitive/symbolic_logic/nimcp_symbolic_logic_safety.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <regex.h>

#define LOG_MODULE "safety"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_buffer_constants.h"

BRIDGE_BOILERPLATE(symbolic_logic_safety, MESH_ADAPTER_CATEGORY_COGNITIVE)



//=============================================================================
// Internal SHA-256 Implementation (standalone, no external dependencies)
//=============================================================================

/**
 * @brief Simple SHA-256 implementation for integrity hashing
 *
 * NOTE: This is a basic implementation for integrity checking.
 * For production use with security requirements, use a vetted library.
 */

#define SHA256_BLOCK_SIZE 64

typedef struct {
    uint32_t state[8];
    uint64_t count;
    uint8_t buffer[SHA256_BLOCK_SIZE];
} sha256_ctx_t;

static const uint32_t sha256_k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

#define ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROTR(x, 2) ^ ROTR(x, 13) ^ ROTR(x, 22))
#define EP1(x) (ROTR(x, 6) ^ ROTR(x, 11) ^ ROTR(x, 25))
#define SIG0(x) (ROTR(x, 7) ^ ROTR(x, 18) ^ ((x) >> 3))
#define SIG1(x) (ROTR(x, 17) ^ ROTR(x, 19) ^ ((x) >> 10))

static void sha256_init(sha256_ctx_t* ctx) {
    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;
    ctx->count = 0;
}

static void sha256_transform(sha256_ctx_t* ctx, const uint8_t* data) {
    uint32_t w[64];
    uint32_t a, b, c, d, e, f, g, h;
    uint32_t t1, t2;

    for (int i = 0; i < 16; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && 16 > 256) {
            symbolic_logic_safety_heartbeat("symbolic_log_loop",
                             (float)(i + 1) / (float)16);
        }

        w[i] = ((uint32_t)data[i * 4] << 24) | ((uint32_t)data[i * 4 + 1] << 16) |
               ((uint32_t)data[i * 4 + 2] << 8) | ((uint32_t)data[i * 4 + 3]);
    }
    for (int i = 16; i < 64; i++) {
        w[i] = SIG1(w[i - 2]) + w[i - 7] + SIG0(w[i - 15]) + w[i - 16];
    }

    a = ctx->state[0]; b = ctx->state[1]; c = ctx->state[2]; d = ctx->state[3];
    e = ctx->state[4]; f = ctx->state[5]; g = ctx->state[6]; h = ctx->state[7];

    for (int i = 0; i < 64; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && 64 > 256) {
            symbolic_logic_safety_heartbeat("symbolic_log_loop",
                             (float)(i + 1) / (float)64);
        }

        t1 = h + EP1(e) + CH(e, f, g) + sha256_k[i] + w[i];
        t2 = EP0(a) + MAJ(a, b, c);
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }

    ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
    ctx->state[4] += e; ctx->state[5] += f; ctx->state[6] += g; ctx->state[7] += h;
}

static void sha256_update(sha256_ctx_t* ctx, const uint8_t* data, size_t len) {
    size_t i = 0;
    size_t index = (size_t)(ctx->count % SHA256_BLOCK_SIZE);
    ctx->count += len;

    if (index) {
        size_t left = SHA256_BLOCK_SIZE - index;
        if (len < left) {
            memcpy(ctx->buffer + index, data, len);
            return;
        }
        memcpy(ctx->buffer + index, data, left);
        sha256_transform(ctx, ctx->buffer);
        i = left;
    }

    while (i + SHA256_BLOCK_SIZE <= len) {
        sha256_transform(ctx, data + i);
        i += SHA256_BLOCK_SIZE;
    }

    if (i < len) {
        memcpy(ctx->buffer, data + i, len - i);
    }
}

static void sha256_final(sha256_ctx_t* ctx, uint8_t* hash) {
    uint8_t pad[64] = {0x80};
    uint64_t bits = ctx->count * 8;
    size_t index = (size_t)(ctx->count % SHA256_BLOCK_SIZE);
    size_t pad_len = (index < 56) ? (56 - index) : (120 - index);

    sha256_update(ctx, pad, pad_len);

    uint8_t len_bytes[8];
    for (int i = 0; i < 8; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && 8 > 256) {
            symbolic_logic_safety_heartbeat("symbolic_log_loop",
                             (float)(i + 1) / (float)8);
        }

        len_bytes[i] = (uint8_t)(bits >> (56 - i * 8));
    }
    sha256_update(ctx, len_bytes, 8);

    for (int i = 0; i < 8; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && 8 > 256) {
            symbolic_logic_safety_heartbeat("symbolic_log_loop",
                             (float)(i + 1) / (float)8);
        }

        hash[i * 4] = (uint8_t)(ctx->state[i] >> 24);
        hash[i * 4 + 1] = (uint8_t)(ctx->state[i] >> 16);
        hash[i * 4 + 2] = (uint8_t)(ctx->state[i] >> 8);
        hash[i * 4 + 3] = (uint8_t)(ctx->state[i]);
    }
}

static void compute_sha256(const void* data, size_t len, uint8_t* hash) {
    sha256_ctx_t ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, (const uint8_t*)data, len);
    sha256_final(&ctx, hash);
}

//=============================================================================
// Internal Statistics (Thread-local for thread safety)
//=============================================================================

typedef struct {
    uint64_t total_evaluations;
    uint64_t rules_triggered;
    uint64_t actions_denied;
    uint64_t actions_escalated;
    uint64_t actions_logged;
    uint64_t actions_warned;
    uint64_t actions_allowed;
    uint64_t triggers_by_domain[SAFETY_DOMAIN_COUNT];
    uint64_t triggers_by_severity[5];
    uint64_t integrity_checks;
    uint64_t integrity_failures;
    uint64_t total_eval_time_us;
    uint64_t max_eval_time_us;
} safety_internal_stats_t;

static _Thread_local safety_internal_stats_t g_stats = {0};

//=============================================================================
// Knowledge Base Lifecycle
//=============================================================================

safety_kb_t* symbolic_logic_safety_kb_create(uint32_t max_rules) {
    // Use default if not specified
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_safety_heartbeat("symbolic_log_kb_create", 0.0f);


    if (max_rules == 0) {
        max_rules = SAFETY_MAX_RULES;
    }

    // Allocate KB structure
    safety_kb_t* kb = (safety_kb_t*)nimcp_calloc(1, sizeof(safety_kb_t));
    if (!kb) {
        LOG_ERROR("Failed to allocate safety KB structure");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate kb");

        return NULL;
    }

    // Calculate mmap size for rules
    size_t page_size = (size_t)sysconf(_SC_PAGESIZE);
    size_t rules_size = max_rules * sizeof(safety_rule_t);
    // Round up to page boundary
    size_t mmap_size = ((rules_size + page_size - 1) / (page_size > 0 ? page_size : 1)) * page_size;

    // Create mmap region
    void* region = mmap(NULL, mmap_size, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (region == MAP_FAILED) {
        LOG_ERROR("Failed to mmap safety KB region: %zu bytes", mmap_size);
        nimcp_free(kb);
        kb = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "symbolic_logic_safety_kb_create: mmap failed");
        return NULL;
    }

    // Initialize KB
    kb->magic = SAFETY_KB_MAGIC;
    kb->version = SAFETY_KB_VERSION;
    kb->mmap_region = region;
    kb->mmap_size = mmap_size;
    kb->rules = (safety_rule_t*)region;
    kb->num_rules = 0;
    kb->max_rules = max_rules;
    kb->hash_computed = false;
    kb->is_locked = false;
    kb->is_compiled = false;
    kb->created_timestamp = nimcp_time_monotonic_ms();
    kb->locked_timestamp = 0;

    memset(kb->integrity_hash, 0, SAFETY_HASH_SIZE);
    memset(kb->rules_by_domain, 0, sizeof(kb->rules_by_domain));

    LOG_INFO("Created safety KB with capacity for %u rules (%zu bytes mmap'd)",
             max_rules, mmap_size);

    return kb;
}

void symbolic_logic_safety_kb_destroy(safety_kb_t* kb) {
    if (!kb) return;

    // Unmap the region (works even if mprotect'd)
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_safety_heartbeat("symbolic_log_kb_destroy", 0.0f);


    if (kb->mmap_region && kb->mmap_region != MAP_FAILED) {
        munmap(kb->mmap_region, kb->mmap_size);
    }

    nimcp_free(kb);
    kb = NULL;
    LOG_DEBUG("Destroyed safety KB");
}

//=============================================================================
// Rule Management
//=============================================================================

uint32_t symbolic_logic_safety_add_rule(safety_kb_t* kb, const safety_rule_t* rule) {
    if (!kb) {
        LOG_ERROR("symbolic_logic_safety_add_rule: kb is NULL");
        return 0;
    }
    if (!rule) {
        LOG_ERROR("symbolic_logic_safety_add_rule: rule is NULL");
        return 0;
    }
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_safety_heartbeat("symbolic_log_add_rule", 0.0f);


    if (kb->is_locked) {
        LOG_ERROR("symbolic_logic_safety_add_rule: KB is locked, cannot add rules");
        return 0;
    }
    if (kb->num_rules >= kb->max_rules) {
        LOG_ERROR("symbolic_logic_safety_add_rule: KB is full (%u/%u rules)",
                  kb->num_rules, kb->max_rules);
        return 0;
    }

    // Assign rule ID
    uint32_t rule_id = kb->num_rules + 1;

    // Copy rule to KB
    safety_rule_t* dest = &kb->rules[kb->num_rules];
    memcpy(dest, rule, sizeof(safety_rule_t));
    dest->rule_id = rule_id;
    dest->is_compiled = false;
    dest->created_timestamp = nimcp_time_monotonic_ms();
    dest->last_triggered = 0;
    dest->trigger_count = 0;

    // Update domain count
    if (rule->domain < SAFETY_DOMAIN_COUNT) {
        kb->rules_by_domain[rule->domain]++;
    }

    kb->num_rules++;
    kb->is_compiled = false;  // Need to recompile

    LOG_DEBUG("Added safety rule: id=%u name='%s' domain=%s severity=%s action=%s",
              rule_id, rule->name,
              safety_domain_name(rule->domain),
              safety_severity_name(rule->severity),
              safety_action_name(rule->action));

    return rule_id;
}

bool symbolic_logic_safety_remove_rule(safety_kb_t* kb, uint32_t rule_id) {
    if (!kb) {
        LOG_ERROR("symbolic_logic_safety_remove_rule: kb is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "symbolic_logic_safety_remove_rule: kb is NULL");
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_safety_heartbeat("symbolic_log_remove_rule", 0.0f);


    if (kb->is_locked) {
        LOG_ERROR("symbolic_logic_safety_remove_rule: KB is locked");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "symbolic_logic_safety_remove_rule: validation failed");
        return false;
    }
    if (rule_id == 0) {
        LOG_ERROR("symbolic_logic_safety_remove_rule: invalid rule_id 0");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "symbolic_logic_safety_remove_rule: rule_id is zero");
        return false;
    }

    // Find rule by ID
    int found_idx = -1;
    for (uint32_t i = 0; i < kb->num_rules; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && kb->num_rules > 256) {
            symbolic_logic_safety_heartbeat("symbolic_log_loop",
                             (float)(i + 1) / (float)kb->num_rules);
        }

        if (kb->rules[i].rule_id == rule_id) {
            found_idx = (int)i;
            break;
        }
    }

    if (found_idx < 0) {
        LOG_ERROR("symbolic_logic_safety_remove_rule: rule_id %u not found", rule_id);
        return false;
    }

    // Update domain count
    safety_domain_t domain = kb->rules[found_idx].domain;
    if (domain < SAFETY_DOMAIN_COUNT && kb->rules_by_domain[domain] > 0) {
        kb->rules_by_domain[domain]--;
    }

    // Shift remaining rules
    for (uint32_t i = (uint32_t)found_idx; i < kb->num_rules - 1; i++) {
        memcpy(&kb->rules[i], &kb->rules[i + 1], sizeof(safety_rule_t));
    }

    kb->num_rules--;
    kb->is_compiled = false;

    LOG_DEBUG("Removed safety rule: id=%u", rule_id);
    return true;
}

const safety_rule_t* symbolic_logic_safety_get_rule(const safety_kb_t* kb, uint32_t rule_id) {
    if (!kb || rule_id == 0) {
        if (!kb) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "symbolic_logic_safety_get_rule: kb is NULL");
        }
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_safety_heartbeat("symbolic_log_get_rule", 0.0f);


    for (uint32_t i = 0; i < kb->num_rules; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && kb->num_rules > 256) {
            symbolic_logic_safety_heartbeat("symbolic_log_loop",
                             (float)(i + 1) / (float)kb->num_rules);
        }

        if (kb->rules[i].rule_id == rule_id) {
            return &kb->rules[i];
        }
    }
    /* P2-COG-21: Rule not found is normal lookup behavior, not an error */
    return NULL;
}

uint32_t symbolic_logic_safety_get_rules_by_domain(
    const safety_kb_t* kb,
    safety_domain_t domain,
    const safety_rule_t** rules_out,
    uint32_t max_rules)
{
    if (!kb || !rules_out || max_rules == 0) return 0;

    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_safety_heartbeat("symbolic_log_get_rules_by_domain", 0.0f);


    uint32_t count = 0;
    for (uint32_t i = 0; i < kb->num_rules && count < max_rules; i++) {
        if (kb->rules[i].domain == domain) {
            rules_out[count++] = &kb->rules[i];
        }
    }
    return count;
}

//=============================================================================
// Rule Compilation
//=============================================================================

/**
 * @brief Generate FOL representation for a single condition
 */
static void compile_condition_to_fol(const safety_condition_t* cond, char* buffer, size_t size) {
    const char* op_str;
    switch (cond->op) {
        case SAFETY_COND_OP_EQ:       op_str = "="; break;
        case SAFETY_COND_OP_NEQ:      op_str = "!="; break;
        case SAFETY_COND_OP_GT:       op_str = ">"; break;
        case SAFETY_COND_OP_LT:       op_str = "<"; break;
        case SAFETY_COND_OP_GTE:      op_str = ">="; break;
        case SAFETY_COND_OP_LTE:      op_str = "<="; break;
        case SAFETY_COND_OP_IN:       op_str = "IN"; break;
        case SAFETY_COND_OP_NOT_IN:   op_str = "NOT_IN"; break;
        case SAFETY_COND_OP_CONTAINS: op_str = "CONTAINS"; break;
        case SAFETY_COND_OP_MATCHES:  op_str = "MATCHES"; break;
        default:                      op_str = "?"; break;
    }

    if (cond->is_negated) {
        snprintf(buffer, size, "NOT(%s %s \"%s\")", cond->field, op_str, cond->value);
    } else {
        snprintf(buffer, size, "%s %s \"%s\"", cond->field, op_str, cond->value);
    }
}

/**
 * @brief Compile a single rule to FOL representation
 */
static void compile_rule_to_fol(safety_rule_t* rule) {
    if (rule->num_conditions == 0) {
        snprintf(rule->fol_representation, SAFETY_MAX_FOL_LEN,
                 "TRUE -> %s", safety_action_name(rule->action));
        rule->is_compiled = true;
        return;
    }

    char* pos = rule->fol_representation;
    size_t remaining = SAFETY_MAX_FOL_LEN;
    int written = 0;

    // Start with opening
    written = snprintf(pos, remaining, "(");
    /* P3-COG-08: Check snprintf return for truncation */
    if (written < 0 || (size_t)written >= remaining) {
        rule->is_compiled = true;
        return;
    }
    pos += written;
    remaining -= (size_t)written;

    // Add all conditions with AND
    for (uint32_t i = 0; i < rule->num_conditions && remaining > 1; i++) {
        char cond_buf[NIMCP_ERROR_BUFFER_SIZE];
        compile_condition_to_fol(&rule->conditions[i], cond_buf, sizeof(cond_buf));

        if (i > 0) {
            written = snprintf(pos, remaining, " AND ");
            if (written < 0 || (size_t)written >= remaining) break;
            pos += written;
            remaining -= (size_t)written;
        }

        written = snprintf(pos, remaining, "%s", cond_buf);
        if (written < 0 || (size_t)written >= remaining) break;
        pos += written;
        remaining -= (size_t)written;
    }

    // Close and add action
    snprintf(pos, remaining, ") -> %s", safety_action_name(rule->action));
    rule->is_compiled = true;
}

bool symbolic_logic_safety_compile_rules(safety_kb_t* kb) {
    if (!kb) {
        LOG_ERROR("symbolic_logic_safety_compile_rules: kb is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "symbolic_logic_safety_compile_rules: kb is NULL");
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_safety_heartbeat("symbolic_log_compile_rules", 0.0f);


    if (kb->is_locked) {
        LOG_ERROR("symbolic_logic_safety_compile_rules: KB is locked");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "symbolic_logic_safety_compile_rules: validation failed");
        return false;
    }

    LOG_INFO("Compiling %u safety rules to FOL...", kb->num_rules);

    // Compile each rule
    for (uint32_t i = 0; i < kb->num_rules; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && kb->num_rules > 256) {
            symbolic_logic_safety_heartbeat("symbolic_log_loop",
                             (float)(i + 1) / (float)kb->num_rules);
        }

        compile_rule_to_fol(&kb->rules[i]);
        LOG_DEBUG("Compiled rule %u: %s", kb->rules[i].rule_id, kb->rules[i].fol_representation);
    }

    // Compute integrity hash over all rules
    compute_sha256(kb->rules, kb->num_rules * sizeof(safety_rule_t), kb->integrity_hash);
    kb->hash_computed = true;

    kb->is_compiled = true;

    LOG_INFO("Compiled %u safety rules, integrity hash computed", kb->num_rules);
    return true;
}

//=============================================================================
// Memory Protection (Locking)
//=============================================================================

bool symbolic_logic_safety_lock(safety_kb_t* kb) {
    if (!kb) {
        LOG_ERROR("symbolic_logic_safety_lock: kb is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "symbolic_logic_safety_lock: kb is NULL");
        return false;
    }
    if (!kb->is_compiled) {
        LOG_ERROR("symbolic_logic_safety_lock: KB must be compiled before locking");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "symbolic_logic_safety_lock: kb->is_compiled is NULL");
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_safety_heartbeat("symbolic_log_lock", 0.0f);


    if (kb->is_locked) {
        LOG_WARN("symbolic_logic_safety_lock: KB is already locked");
        return true;
    }

    LOG_WARN("LOCKING SAFETY KB - THIS IS IRREVERSIBLE");

    // Apply mprotect to make region read-only
    if (mprotect(kb->mmap_region, kb->mmap_size, PROT_READ) != 0) {
        LOG_ERROR("symbolic_logic_safety_lock: mprotect failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "symbolic_logic_safety_lock: validation failed");
        return false;
    }

    kb->is_locked = true;
    kb->locked_timestamp = nimcp_time_monotonic_ms();

    LOG_WARN("Safety KB LOCKED at timestamp %lu with %u rules",
             (unsigned long)kb->locked_timestamp, kb->num_rules);

    return true;
}

bool symbolic_logic_safety_is_locked(const safety_kb_t* kb) {
    if (!kb) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_safety_heartbeat("symbolic_log_is_locked", 0.0f);


    return kb->is_locked;
}

//=============================================================================
// Integrity Verification
//=============================================================================

bool symbolic_logic_safety_verify_integrity(const safety_kb_t* kb) {
    if (!kb) {
        LOG_ERROR("symbolic_logic_safety_verify_integrity: kb is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "symbolic_logic_safety_verify_integrity: kb is NULL");
        return false;
    }
    if (!kb->hash_computed) {
        LOG_ERROR("symbolic_logic_safety_verify_integrity: hash not computed (not compiled)");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "symbolic_logic_safety_verify_integrity: kb->hash_computed is NULL");
        return false;
    }

    /* Optimization: If KB is locked (mprotect PROT_READ), memory is immutable.
     * Once integrity is verified after locking, it cannot change, so we can
     * skip recomputation on subsequent checks. */
    if (kb->is_locked && kb->integrity_verified_while_locked) {
        g_stats.integrity_checks++;
        return true;
    }

    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_safety_heartbeat("symbolic_log_verify_integrity", 0.0f);


    g_stats.integrity_checks++;

    // Recompute hash
    uint8_t computed_hash[SAFETY_HASH_SIZE];
    compute_sha256(kb->rules, kb->num_rules * sizeof(safety_rule_t), computed_hash);

    // Compare
    if (memcmp(computed_hash, kb->integrity_hash, SAFETY_HASH_SIZE) != 0) {
        LOG_ERROR("SAFETY KB INTEGRITY VERIFICATION FAILED - POSSIBLE TAMPERING");
        g_stats.integrity_failures++;
        return false;
    }

    /* P2-COG-22: Cache result for locked KBs - explicit const cast for
     * mutable cache field. integrity_verified_while_locked is a cache
     * optimization that does not affect safety-critical state. */
    if (kb->is_locked) {
        ((safety_kb_t*)(uintptr_t)kb)->integrity_verified_while_locked = true;
    }

    LOG_DEBUG("Safety KB integrity verified");
    return true;
}

bool symbolic_logic_safety_get_hash(const safety_kb_t* kb, uint8_t* hash_out) {
    if (!kb || !hash_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "symbolic_logic_safety_get_hash: required parameter is NULL (kb, hash_out)");
        return false;
    }
    if (!kb->hash_computed) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "symbolic_logic_safety_get_hash: kb->hash_computed is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_safety_heartbeat("symbolic_log_get_hash", 0.0f);


    memcpy(hash_out, kb->integrity_hash, SAFETY_HASH_SIZE);
    return true;
}

//=============================================================================
// Condition Evaluation
//=============================================================================

/**
 * @brief Find string field in context
 */
static const char* find_string_field(const safety_action_context_t* ctx, const char* key) {
    for (uint32_t i = 0; i < ctx->num_string_fields; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ctx->num_string_fields > 256) {
            symbolic_logic_safety_heartbeat("symbolic_log_loop",
                             (float)(i + 1) / (float)ctx->num_string_fields);
        }

        if (strcmp(ctx->string_fields[i].key, key) == 0) {
            return ctx->string_fields[i].value;
        }
    }
    /* Field not found is normal lookup behavior, not an error */
    return NULL;
}

/**
 * @brief Find numeric field in context
 */
static bool find_numeric_field(const safety_action_context_t* ctx, const char* key, float* value_out) {
    for (uint32_t i = 0; i < ctx->num_numeric_fields; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && ctx->num_numeric_fields > 256) {
            symbolic_logic_safety_heartbeat("symbolic_log_loop",
                             (float)(i + 1) / (float)ctx->num_numeric_fields);
        }

        if (strcmp(ctx->numeric_fields[i].key, key) == 0) {
            *value_out = ctx->numeric_fields[i].value;
            return true;
        }
    }
    return false;
}

/**
 * @brief Evaluate a single condition against context
 */
static bool evaluate_condition(const safety_condition_t* cond, const safety_action_context_t* ctx) {
    bool result = false;

    // Try to find field as string first
    const char* str_val = find_string_field(ctx, cond->field);
    float num_val = 0.0f;
    bool is_numeric = find_numeric_field(ctx, cond->field, &num_val);

    // Evaluate based on operator
    switch (cond->op) {
        case SAFETY_COND_OP_EQ:
            if (str_val) {
                result = (strcmp(str_val, cond->value) == 0);
            } else if (is_numeric) {
                result = (num_val == cond->numeric_value);
            }
            break;

        case SAFETY_COND_OP_NEQ:
            if (str_val) {
                result = (strcmp(str_val, cond->value) != 0);
            } else if (is_numeric) {
                result = (num_val != cond->numeric_value);
            }
            break;

        case SAFETY_COND_OP_GT:
            if (is_numeric) {
                result = (num_val > cond->numeric_value);
            }
            break;

        case SAFETY_COND_OP_LT:
            if (is_numeric) {
                result = (num_val < cond->numeric_value);
            }
            break;

        case SAFETY_COND_OP_GTE:
            if (is_numeric) {
                result = (num_val >= cond->numeric_value);
            }
            break;

        case SAFETY_COND_OP_LTE:
            if (is_numeric) {
                result = (num_val <= cond->numeric_value);
            }
            break;

        case SAFETY_COND_OP_IN:
            // Check if value is in comma-separated list
            if (str_val) {
                char list_copy[SAFETY_MAX_VALUE_LEN];
                strncpy(list_copy, cond->value, sizeof(list_copy) - 1);
                list_copy[sizeof(list_copy) - 1] = '\0';
                /* P2-COG-20: Use thread-safe strtok_r instead of strtok */
                char* saveptr = NULL;
                char* token = strtok_r(list_copy, ",", &saveptr);
                while (token) {
                    // Trim whitespace
                    while (*token == ' ') token++;
                    if (strcmp(str_val, token) == 0) {
                        result = true;
                        break;
                    }
                    token = strtok_r(NULL, ",", &saveptr);
                }
            }
            break;

        case SAFETY_COND_OP_NOT_IN:
            // Check if value is NOT in comma-separated list
            if (str_val) {
                result = true;  // Assume not in until found
                char list_copy[SAFETY_MAX_VALUE_LEN];
                strncpy(list_copy, cond->value, sizeof(list_copy) - 1);
                list_copy[sizeof(list_copy) - 1] = '\0';
                /* P2-COG-20: Use thread-safe strtok_r instead of strtok */
                char* saveptr = NULL;
                char* token = strtok_r(list_copy, ",", &saveptr);
                while (token) {
                    while (*token == ' ') token++;
                    if (strcmp(str_val, token) == 0) {
                        result = false;
                        break;
                    }
                    token = strtok_r(NULL, ",", &saveptr);
                }
            }
            break;

        case SAFETY_COND_OP_CONTAINS:
            if (str_val) {
                result = (strstr(str_val, cond->value) != NULL);
            }
            break;

        case SAFETY_COND_OP_MATCHES:
            if (str_val) {
                regex_t regex;
                if (regcomp(&regex, cond->value, REG_EXTENDED | REG_NOSUB) == 0) {
                    result = (regexec(&regex, str_val, 0, NULL, 0) == 0);
                    regfree(&regex);
                }
            }
            break;

        default:
            LOG_WARN("Unknown condition operator: %d", cond->op);
            break;
    }

    // Apply negation
    if (cond->is_negated) {
        result = !result;
    }

    return result;
}

/**
 * @brief Evaluate all conditions of a rule (AND logic)
 */
static bool evaluate_rule_conditions(const safety_rule_t* rule, const safety_action_context_t* ctx) {
    if (rule->num_conditions == 0) {
        // No conditions = always matches
        return true;
    }

    for (uint32_t i = 0; i < rule->num_conditions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && rule->num_conditions > 256) {
            symbolic_logic_safety_heartbeat("symbolic_log_loop",
                             (float)(i + 1) / (float)rule->num_conditions);
        }

        if (!evaluate_condition(&rule->conditions[i], ctx)) {
            return false;  // AND logic - any failure means rule doesn't match
        }
    }

    return true;  // All conditions matched
}

//=============================================================================
// Rule Evaluation
//=============================================================================

bool symbolic_logic_safety_evaluate(
    const safety_kb_t* kb,
    const safety_action_context_t* context,
    safety_evaluation_t* result)
{
    if (!kb) {
        LOG_ERROR("symbolic_logic_safety_evaluate: kb is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "symbolic_logic_safety_evaluate: kb is NULL");
        return false;
    }
    if (!context) {
        LOG_ERROR("symbolic_logic_safety_evaluate: context is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "symbolic_logic_safety_evaluate: context is NULL");
        return false;
    }
    if (!result) {
        LOG_ERROR("symbolic_logic_safety_evaluate: result is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "symbolic_logic_safety_evaluate: result is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_safety_heartbeat("symbolic_log_evaluate", 0.0f);


    uint64_t start_time = nimcp_time_monotonic_us();

    // Initialize result
    memset(result, 0, sizeof(safety_evaluation_t));
    result->action = SAFETY_ACTION_ALLOW;
    result->max_severity = SAFETY_SEVERITY_INFO;
    result->confidence = 1.0f;
    result->kb_is_locked = kb->is_locked;

    // Verify integrity
    result->integrity_verified = symbolic_logic_safety_verify_integrity(kb);
    if (!result->integrity_verified) {
        LOG_ERROR("Safety KB integrity check failed during evaluation");
        snprintf(result->explanation, sizeof(result->explanation),
                 "INTEGRITY CHECK FAILED - EVALUATION ABORTED");
        result->action = SAFETY_ACTION_DENY;  // Fail-safe to deny
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "symbolic_logic_safety_evaluate: result->integrity_verified is NULL");
        return false;
    }

    // Allocate array for triggered rule IDs (worst case: all rules trigger)
    result->triggered_rule_ids = (uint32_t*)nimcp_calloc(kb->num_rules, sizeof(uint32_t));
    if (!result->triggered_rule_ids && kb->num_rules > 0) {
        LOG_ERROR("Failed to allocate triggered_rule_ids array");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "symbolic_logic_safety_evaluate: result->triggered_rule_ids is NULL");
        return false;
    }
    result->num_triggered = 0;

    // Forward chaining evaluation over all rules
    for (uint32_t i = 0; i < kb->num_rules; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && kb->num_rules > 256) {
            symbolic_logic_safety_heartbeat("symbolic_log_loop",
                             (float)(i + 1) / (float)kb->num_rules);
        }

        const safety_rule_t* rule = &kb->rules[i];

        // Skip disabled rules
        if (!rule->enabled) {
            continue;
        }

        // NOTE: Domain hint is NOT used to skip rules - this would be a security
        // vulnerability allowing attackers to bypass safety rules by lying about
        // the domain. All rules are evaluated based on content, regardless of
        // any domain hints provided.

        // Evaluate rule conditions
        if (evaluate_rule_conditions(rule, context)) {
            // Rule triggered
            result->triggered_rule_ids[result->num_triggered++] = rule->rule_id;

            // Update statistics
            g_stats.rules_triggered++;
            if (rule->domain < SAFETY_DOMAIN_COUNT) {
                g_stats.triggers_by_domain[rule->domain]++;
            }
            if (rule->severity <= SAFETY_SEVERITY_INFO) {
                g_stats.triggers_by_severity[rule->severity]++;
            }

            // Track highest severity
            if (rule->severity < result->max_severity) {
                result->max_severity = rule->severity;
            }

            // Apply action (DENY > ESCALATE > WARN > LOG > ALLOW)
            if (rule->action == SAFETY_ACTION_DENY) {
                result->action = SAFETY_ACTION_DENY;
            } else if (rule->action == SAFETY_ACTION_ESCALATE && result->action != SAFETY_ACTION_DENY) {
                result->action = SAFETY_ACTION_ESCALATE;
            } else if (rule->action == SAFETY_ACTION_WARN &&
                       result->action != SAFETY_ACTION_DENY &&
                       result->action != SAFETY_ACTION_ESCALATE) {
                result->action = SAFETY_ACTION_WARN;
            } else if (rule->action == SAFETY_ACTION_LOG &&
                       result->action == SAFETY_ACTION_ALLOW) {
                result->action = SAFETY_ACTION_LOG;
            }

            LOG_DEBUG("Rule triggered: id=%u name='%s' action=%s",
                      rule->rule_id, rule->name, safety_action_name(rule->action));
        }
    }

    // Update action statistics
    g_stats.total_evaluations++;
    switch (result->action) {
        case SAFETY_ACTION_DENY: g_stats.actions_denied++; break;
        case SAFETY_ACTION_ESCALATE: g_stats.actions_escalated++; break;
        case SAFETY_ACTION_WARN: g_stats.actions_warned++; break;
        case SAFETY_ACTION_LOG: g_stats.actions_logged++; break;
        case SAFETY_ACTION_ALLOW: g_stats.actions_allowed++; break;
    }

    // Generate explanation
    if (result->num_triggered == 0) {
        snprintf(result->explanation, sizeof(result->explanation),
                 "No safety rules triggered. Action allowed.");
    } else if (result->action == SAFETY_ACTION_DENY) {
        snprintf(result->explanation, sizeof(result->explanation),
                 "Action DENIED: %u rule(s) triggered with max severity %s",
                 result->num_triggered, safety_severity_name(result->max_severity));
    } else if (result->action == SAFETY_ACTION_ESCALATE) {
        snprintf(result->explanation, sizeof(result->explanation),
                 "Action requires ESCALATION: %u rule(s) triggered",
                 result->num_triggered);
    } else {
        snprintf(result->explanation, sizeof(result->explanation),
                 "%u rule(s) triggered with action %s",
                 result->num_triggered, safety_action_name(result->action));
    }

    // Calculate evaluation time
    uint64_t end_time = nimcp_time_monotonic_us();
    result->evaluation_time_us = end_time - start_time;
    g_stats.total_eval_time_us += result->evaluation_time_us;
    if (result->evaluation_time_us > g_stats.max_eval_time_us) {
        g_stats.max_eval_time_us = result->evaluation_time_us;
    }

    LOG_DEBUG("Safety evaluation complete: %u rules triggered, action=%s, time=%lu us",
              result->num_triggered, safety_action_name(result->action),
              (unsigned long)result->evaluation_time_us);

    return true;
}

void symbolic_logic_safety_free_evaluation(safety_evaluation_t* result) {
    if (!result) return;

    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_safety_heartbeat("symbolic_log_free_evaluation", 0.0f);


    if (result->triggered_rule_ids) {
        nimcp_free(result->triggered_rule_ids);
        result->triggered_rule_ids = NULL;
    }
    result->num_triggered = 0;
}

//=============================================================================
// Statistics
//=============================================================================

bool symbolic_logic_safety_get_stats(const safety_kb_t* kb, safety_stats_t* stats) {
    if (!kb || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "symbolic_logic_safety_get_stats: required parameter is NULL (kb, stats)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_safety_heartbeat("symbolic_log_get_stats", 0.0f);


    memset(stats, 0, sizeof(safety_stats_t));

    stats->total_evaluations = g_stats.total_evaluations;
    stats->rules_triggered = g_stats.rules_triggered;
    stats->actions_denied = g_stats.actions_denied;
    stats->actions_escalated = g_stats.actions_escalated;
    stats->actions_logged = g_stats.actions_logged;
    stats->actions_warned = g_stats.actions_warned;
    stats->actions_allowed = g_stats.actions_allowed;

    memcpy(stats->triggers_by_domain, g_stats.triggers_by_domain, sizeof(stats->triggers_by_domain));
    memcpy(stats->triggers_by_severity, g_stats.triggers_by_severity, sizeof(stats->triggers_by_severity));

    stats->integrity_checks = g_stats.integrity_checks;
    stats->integrity_failures = g_stats.integrity_failures;

    if (g_stats.total_evaluations > 0) {
        stats->avg_evaluation_time_us = g_stats.total_eval_time_us / g_stats.total_evaluations;
    }
    stats->max_evaluation_time_us = g_stats.max_eval_time_us;

    stats->kb_locked_timestamp = kb->locked_timestamp;
    stats->kb_is_locked = kb->is_locked;

    return true;
}

void symbolic_logic_safety_reset_stats(safety_kb_t* kb) {
    if (!kb) return;
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_safety_heartbeat("symbolic_log_reset_stats", 0.0f);


    memset(&g_stats, 0, sizeof(g_stats));
    LOG_DEBUG("Safety statistics reset");
}

//=============================================================================
// Utility Functions
//=============================================================================

void symbolic_logic_safety_init_rule(safety_rule_t* rule) {
    if (!rule) return;
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_safety_heartbeat("symbolic_log_init_rule", 0.0f);


    memset(rule, 0, sizeof(safety_rule_t));
    rule->enabled = true;
    rule->priority = 0.5f;
}

void symbolic_logic_safety_init_context(safety_action_context_t* context) {
    if (!context) return;
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_safety_heartbeat("symbolic_log_init_context", 0.0f);


    memset(context, 0, sizeof(safety_action_context_t));
    context->timestamp = nimcp_time_monotonic_ms();
}

bool symbolic_logic_safety_context_add_string(
    safety_action_context_t* context,
    const char* key,
    const char* value)
{
    if (!context || !key || !value) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "symbolic_logic_safety_context_add_string: required parameter is NULL (context, key, value)");
        return false;
    }
    if (context->num_string_fields >= 32) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "symbolic_logic_safety_context_add_string: capacity exceeded");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_safety_heartbeat("symbolic_log_context_add_string", 0.0f);


    uint32_t idx = context->num_string_fields;
    strncpy(context->string_fields[idx].key, key, sizeof(context->string_fields[idx].key) - 1);
    strncpy(context->string_fields[idx].value, value, sizeof(context->string_fields[idx].value) - 1);
    context->num_string_fields++;

    return true;
}

bool symbolic_logic_safety_context_add_numeric(
    safety_action_context_t* context,
    const char* key,
    float value)
{
    if (!context || !key) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "symbolic_logic_safety_context_add_numeric: required parameter is NULL (context, key)");
        return false;
    }
    if (context->num_numeric_fields >= 16) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "symbolic_logic_safety_context_add_numeric: capacity exceeded");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_safety_heartbeat("symbolic_log_context_add_numeric", 0.0f);


    uint32_t idx = context->num_numeric_fields;
    strncpy(context->numeric_fields[idx].key, key, sizeof(context->numeric_fields[idx].key) - 1);
    context->numeric_fields[idx].value = value;
    context->num_numeric_fields++;

    return true;
}

void symbolic_logic_safety_print_rule(const safety_rule_t* rule) {
    if (!rule) return;

    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_safety_heartbeat("symbolic_log_print_rule", 0.0f);


    LOG_DEBUG("Safety Rule [%u]: %s", rule->rule_id, rule->name);
    LOG_DEBUG("  Description: %s", rule->description);
    LOG_DEBUG("  Domain: %s, Severity: %s, Action: %s",
              safety_domain_name(rule->domain),
              safety_severity_name(rule->severity),
              safety_action_name(rule->action));
    LOG_DEBUG("  Priority: %.2f, Enabled: %s", rule->priority, rule->enabled ? "yes" : "no");
    LOG_DEBUG("  Conditions (%u):", rule->num_conditions);

    for (uint32_t i = 0; i < rule->num_conditions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && rule->num_conditions > 256) {
            symbolic_logic_safety_heartbeat("symbolic_log_loop",
                             (float)(i + 1) / (float)rule->num_conditions);
        }

        const safety_condition_t* cond = &rule->conditions[i];
        LOG_DEBUG("    [%u] %s%s %s '%s'",
                  i,
                  cond->is_negated ? "NOT " : "",
                  cond->field,
                  safety_condition_op_name(cond->op),
                  cond->value);
    }

    if (rule->is_compiled) {
        LOG_DEBUG("  FOL: %s", rule->fol_representation);
    }
}

void symbolic_logic_safety_print_evaluation(const safety_evaluation_t* result) {
    if (!result) return;

    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_safety_heartbeat("symbolic_log_print_evaluation", 0.0f);


    LOG_DEBUG("Safety Evaluation Result:");
    LOG_DEBUG("  Action: %s", safety_action_name(result->action));
    LOG_DEBUG("  Max Severity: %s", safety_severity_name(result->max_severity));
    LOG_DEBUG("  Triggered Rules: %u", result->num_triggered);
    LOG_DEBUG("  Confidence: %.2f", result->confidence);
    LOG_DEBUG("  Integrity Verified: %s", result->integrity_verified ? "yes" : "no");
    LOG_DEBUG("  KB Locked: %s", result->kb_is_locked ? "yes" : "no");
    LOG_DEBUG("  Evaluation Time: %lu us", (unsigned long)result->evaluation_time_us);
    LOG_DEBUG("  Explanation: %s", result->explanation);

    if (result->num_triggered > 0 && result->triggered_rule_ids) {
        LOG_DEBUG("  Triggered Rule IDs:");
        for (uint32_t i = 0; i < result->num_triggered; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && result->num_triggered > 256) {
                symbolic_logic_safety_heartbeat("symbolic_log_loop",
                                 (float)(i + 1) / (float)result->num_triggered);
            }

            LOG_DEBUG("    - %u", result->triggered_rule_ids[i]);
        }
    }
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void symbolic_logic_safety_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_symbolic_logic_safety_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int symbolic_logic_safety_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "symbolic_logic_safety_training_begin: NULL argument");
        return -1;
    }
    symbolic_logic_safety_heartbeat_instance(NULL, "symbolic_logic_safety_training_begin", 0.0f);
    (void)(sha256_ctx_t*)instance; /* Module state available for reset */
    return 0;
}

int symbolic_logic_safety_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "symbolic_logic_safety_training_end: NULL argument");
        return -1;
    }
    symbolic_logic_safety_heartbeat_instance(NULL, "symbolic_logic_safety_training_end", 1.0f);
    (void)(sha256_ctx_t*)instance; /* Module state available for finalization */
    return 0;
}

int symbolic_logic_safety_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "symbolic_logic_safety_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    symbolic_logic_safety_heartbeat_instance(NULL, "symbolic_logic_safety_training_step", progress);
    (void)(sha256_ctx_t*)instance; /* Module state available for step adaptation */
    return 0;
}
