/**
 * @file nimcp_reasoning_kb_persistence.c
 * @brief Knowledge Base Persistence — serialization/deserialization implementation
 *
 * SINGLE RESPONSIBILITY: Export and import KB facts/rules to/from buffers and files
 *
 * WHAT: Implement binary and text serialization of qreason_kb_t knowledge bases
 * WHY:  Enable persistence of derived facts and rules between sessions
 * HOW:  Binary layout: [kb_header_t][kb_fact_entry_t * N][kb_rule_entry_t * M]
 *        Text layout:  human-readable line-based format
 *        Checksum: FNV-1a 32-bit over data section
 *
 * @author NIMCP Development Team
 * @date 2026-02-26
 * @version 1.0.0
 */

#include "cognitive/reasoning/nimcp_reasoning_kb_persistence.h"
#include "cognitive/reasoning/nimcp_quantum_reasoning.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

#include <string.h>
#include <stdio.h>
#include <time.h>

/*=============================================================================
 * FNV-1a 32-BIT HASH
 *===========================================================================*/

/** FNV offset basis (32-bit) */
#define FNV_OFFSET_BASIS 0x811C9DC5u

/** FNV prime (32-bit) */
#define FNV_PRIME 0x01000193u

/**
 * @brief Compute FNV-1a 32-bit hash
 *
 * WHAT: Hash a byte array with FNV-1a
 * WHY:  Fast, well-distributed checksum for integrity verification
 * HOW:  XOR-fold each byte and multiply by prime
 */
static uint32_t fnv1a_hash(const uint8_t* data, size_t len)
{
    if (!data || len == 0) return 0;

    uint32_t hash = FNV_OFFSET_BASIS;
    for (size_t i = 0; i < len; i++) {
        hash ^= data[i];
        hash *= FNV_PRIME;
    }
    return hash;
}

/*=============================================================================
 * TRUTH VALUE HELPERS
 *===========================================================================*/

/**
 * @brief Convert qreason_truth_t to serialized uint8_t
 *
 * WHAT: Map ternary truth (-1,0,+1) to (0,2,1) for storage
 */
static uint8_t truth_to_byte(int8_t truth)
{
    if (truth == 1)  return 1;  /* TRUE */
    if (truth == -1) return 0;  /* FALSE */
    return 2;                    /* UNKNOWN */
}

/**
 * @brief Convert serialized uint8_t back to qreason_truth_t
 */
static int8_t byte_to_truth(uint8_t b)
{
    if (b == 1) return 1;   /* TRUE */
    if (b == 0) return -1;  /* FALSE */
    return 0;                /* UNKNOWN */
}

/*=============================================================================
 * TEXT FORMAT HELPERS
 *===========================================================================*/

/**
 * @brief Get truth character for text format
 */
static char truth_char(int8_t truth)
{
    if (truth == 1)  return 'T';
    if (truth == -1) return 'F';
    return 'U';
}

/**
 * @brief Parse truth character from text format
 */
static int8_t parse_truth_char(char c)
{
    if (c == 'T') return 1;
    if (c == 'F') return -1;
    return 0;
}

/*=============================================================================
 * DEFAULT CONFIGURATION
 *===========================================================================*/

kb_persistence_config_t reasoning_kb_default_config(void)
{
    kb_persistence_config_t config;
    memset(&config, 0, sizeof(config));
    config.format = KB_FORMAT_BINARY;
    config.include_derived_facts = true;
    config.include_confidences = true;
    config.compress = false;
    return config;
}

/*=============================================================================
 * BINARY EXPORT
 *===========================================================================*/

/**
 * @brief Export KB to binary buffer
 *
 * WHAT: Serialize qreason_kb_t into binary format
 * WHY:  Compact, fast to parse
 * HOW:  Write header, fact entries, rule entries; compute checksum
 */
static int export_binary(
    const qreason_t ctx,
    const kb_persistence_config_t* config,
    uint8_t** buffer_out,
    size_t* size_out)
{
    if (!ctx) return -1;

    /* Count facts (non-UNKNOWN variables) */
    uint32_t num_facts = 0;
    for (uint32_t i = 0; i < ctx->kb.n_variables; i++) {
        if (ctx->kb.facts[i] != 0) {  /* QREASON_UNKNOWN = 0 */
            num_facts++;
        }
    }

    uint32_t num_rules = ctx->kb.n_rules;

    /* Calculate buffer size */
    size_t data_size = (size_t)num_facts * sizeof(kb_fact_entry_t)
                     + (size_t)num_rules * sizeof(kb_rule_entry_t);
    size_t total_size = sizeof(kb_header_t) + data_size;

    /* Allocate buffer */
    uint8_t* buf = (uint8_t*)nimcp_calloc(1, total_size);
    if (!buf) return -1;

    /* Write header (checksum filled in later) */
    kb_header_t* header = (kb_header_t*)buf;
    header->magic = KB_PERSISTENCE_MAGIC;
    header->version = KB_PERSISTENCE_VERSION;
    header->num_facts = num_facts;
    header->num_rules = num_rules;
    header->timestamp = (uint64_t)time(NULL);
    header->checksum = 0;  /* Placeholder */

    /* Write fact entries */
    uint8_t* ptr = buf + sizeof(kb_header_t);
    for (uint32_t i = 0; i < ctx->kb.n_variables; i++) {
        if (ctx->kb.facts[i] == 0) continue;  /* Skip UNKNOWN */

        kb_fact_entry_t* entry = (kb_fact_entry_t*)ptr;
        entry->variable_id = i;
        entry->truth_value = truth_to_byte(ctx->kb.facts[i]);
        entry->confidence = config->include_confidences
                            ? ctx->kb.confidences[i] : 0.0f;
        snprintf(entry->description, KB_MAX_FACT_LEN, "var_%u", i);
        entry->is_derived = false;  /* Cannot distinguish in qreason_kb_t */
        ptr += sizeof(kb_fact_entry_t);
    }

    /* Write rule entries */
    for (uint32_t i = 0; i < num_rules; i++) {
        kb_rule_entry_t* entry = (kb_rule_entry_t*)ptr;
        const qreason_rule_t* rule = &ctx->kb.rules[i];

        entry->num_antecedents = rule->n_antecedents;
        if (entry->num_antecedents > 8) entry->num_antecedents = 8;

        for (uint32_t j = 0; j < entry->num_antecedents; j++) {
            entry->antecedents[j] = rule->antecedents[j];
        }

        entry->consequent = rule->consequent;
        entry->confidence = config->include_confidences
                            ? rule->confidence : 0.0f;
        snprintf(entry->description, KB_MAX_RULE_LEN, "rule_%u", i);
        ptr += sizeof(kb_rule_entry_t);
    }

    /* Compute checksum over data section (after header) */
    uint8_t* data_start = buf + sizeof(kb_header_t);
    size_t data_len = (size_t)(ptr - data_start);
    header->checksum = fnv1a_hash(data_start, data_len);

    *buffer_out = buf;
    *size_out = total_size;
    return 0;
}

/*=============================================================================
 * TEXT EXPORT
 *===========================================================================*/

/**
 * @brief Export KB to text buffer
 *
 * WHAT: Serialize qreason_kb_t into human-readable text
 * WHY:  Enable inspection and debugging
 * HOW:  Line-based format with labeled sections
 */
static int export_text(
    const qreason_t ctx,
    const kb_persistence_config_t* config,
    uint8_t** buffer_out,
    size_t* size_out)
{
    if (!ctx) return -1;

    /* Count facts */
    uint32_t num_facts = 0;
    for (uint32_t i = 0; i < ctx->kb.n_variables; i++) {
        if (ctx->kb.facts[i] != 0) num_facts++;
    }

    uint32_t num_rules = ctx->kb.n_rules;

    /* Estimate buffer size (generous) */
    size_t est_size = 256 + (size_t)num_facts * 320 + (size_t)num_rules * 600;
    char* text = (char*)nimcp_calloc(1, est_size);
    if (!text) return -1;

    size_t pos = 0;

    /* Header lines */
    pos += (size_t)snprintf(text + pos, est_size - pos,
        "# NIMCP Knowledge Base v%u\n", KB_PERSISTENCE_VERSION);
    pos += (size_t)snprintf(text + pos, est_size - pos,
        "# Facts: %u, Rules: %u\n", num_facts, num_rules);
    pos += (size_t)snprintf(text + pos, est_size - pos,
        "# Timestamp: %lu\n\n", (unsigned long)time(NULL));

    /* Facts section */
    pos += (size_t)snprintf(text + pos, est_size - pos, "[FACTS]\n");
    for (uint32_t i = 0; i < ctx->kb.n_variables; i++) {
        if (ctx->kb.facts[i] == 0) continue;

        float conf = config->include_confidences ? ctx->kb.confidences[i] : 0.0f;
        pos += (size_t)snprintf(text + pos, est_size - pos,
            "F %u %c %.4f 0 var_%u\n",
            i, truth_char(ctx->kb.facts[i]), conf, i);
    }

    /* Rules section */
    pos += (size_t)snprintf(text + pos, est_size - pos, "\n[RULES]\n");
    for (uint32_t i = 0; i < num_rules; i++) {
        const qreason_rule_t* rule = &ctx->kb.rules[i];
        pos += (size_t)snprintf(text + pos, est_size - pos, "R ");

        for (uint32_t j = 0; j < rule->n_antecedents; j++) {
            pos += (size_t)snprintf(text + pos, est_size - pos,
                "%u ", rule->antecedents[j]);
        }

        float conf = config->include_confidences ? rule->confidence : 0.0f;
        pos += (size_t)snprintf(text + pos, est_size - pos,
            "-> %u %.4f rule_%u\n", rule->consequent, conf, i);
    }

    *buffer_out = (uint8_t*)text;
    *size_out = pos;
    return 0;
}

/*=============================================================================
 * EXPORT TO BUFFER
 *===========================================================================*/

int reasoning_kb_export_to_buffer(
    const void* kb_source,
    const kb_persistence_config_t* config,
    uint8_t** buffer_out,
    size_t* size_out)
{
    /* Guard clauses */
    if (!kb_source) return -1;
    if (!buffer_out) return -1;
    if (!size_out) return -1;

    /* Use defaults if no config */
    kb_persistence_config_t actual_config;
    if (config) {
        actual_config = *config;
    } else {
        actual_config = reasoning_kb_default_config();
    }

    const qreason_t ctx = (const qreason_t)kb_source;

    if (actual_config.format == KB_FORMAT_TEXT) {
        return export_text(ctx, &actual_config, buffer_out, size_out);
    }

    return export_binary(ctx, &actual_config, buffer_out, size_out);
}

/*=============================================================================
 * IMPORT FROM BUFFER
 *===========================================================================*/

/**
 * @brief Import from binary buffer
 */
static int import_binary(
    qreason_t ctx,
    const uint8_t* buffer,
    size_t size,
    kb_import_result_t* result)
{
    /* Validate first */
    if (reasoning_kb_validate_buffer(buffer, size) != 0) return -1;

    const kb_header_t* header = (const kb_header_t*)buffer;
    const uint8_t* ptr = buffer + sizeof(kb_header_t);

    uint32_t facts_imported = 0;
    uint32_t rules_imported = 0;
    uint32_t conflicts = 0;

    /* Import facts */
    for (uint32_t i = 0; i < header->num_facts; i++) {
        const kb_fact_entry_t* entry = (const kb_fact_entry_t*)ptr;
        uint32_t var_id = entry->variable_id;

        if (var_id >= QREASON_MAX_VARIABLES) {
            ptr += sizeof(kb_fact_entry_t);
            continue;
        }

        int8_t new_truth = byte_to_truth(entry->truth_value);
        float new_conf = entry->confidence;

        /* Conflict resolution: higher confidence wins */
        if (ctx->kb.facts[var_id] != 0 && ctx->kb.facts[var_id] != new_truth) {
            if (new_conf > ctx->kb.confidences[var_id]) {
                ctx->kb.facts[var_id] = new_truth;
                ctx->kb.confidences[var_id] = new_conf;
            }
            conflicts++;
        } else {
            ctx->kb.facts[var_id] = new_truth;
            ctx->kb.confidences[var_id] = new_conf;
        }

        /* Track n_variables */
        if (var_id + 1 > ctx->kb.n_variables) {
            ctx->kb.n_variables = var_id + 1;
        }

        facts_imported++;
        ptr += sizeof(kb_fact_entry_t);
    }

    /* Import rules */
    for (uint32_t i = 0; i < header->num_rules; i++) {
        const kb_rule_entry_t* entry = (const kb_rule_entry_t*)ptr;

        if (ctx->kb.n_rules < QREASON_MAX_RULES) {
            qreason_rule_t* rule = &ctx->kb.rules[ctx->kb.n_rules];
            rule->n_antecedents = entry->num_antecedents;
            if (rule->n_antecedents > 8) rule->n_antecedents = 8;

            for (uint32_t j = 0; j < rule->n_antecedents; j++) {
                rule->antecedents[j] = entry->antecedents[j];
            }

            rule->consequent = entry->consequent;
            rule->confidence = entry->confidence;
            ctx->kb.n_rules++;
            rules_imported++;
        }

        ptr += sizeof(kb_rule_entry_t);
    }

    /* Fill result */
    if (result) {
        result->num_facts_imported = facts_imported;
        result->num_rules_imported = rules_imported;
        result->num_conflicts = conflicts;
        result->bytes_read = size;
    }

    return 0;
}

/**
 * @brief Import from text buffer
 */
static int import_text(
    qreason_t ctx,
    const uint8_t* buffer,
    size_t size,
    kb_import_result_t* result)
{
    /* Work with a NUL-terminated copy */
    char* text = (char*)nimcp_calloc(1, size + 1);
    if (!text) return -1;
    memcpy(text, buffer, size);
    text[size] = '\0';

    uint32_t facts_imported = 0;
    uint32_t rules_imported = 0;
    uint32_t conflicts = 0;

    char* line = text;
    while (line && *line) {
        /* Find end of line */
        char* eol = strchr(line, '\n');
        if (eol) *eol = '\0';

        /* Skip comments and empty lines */
        if (line[0] == '#' || line[0] == '\0' || line[0] == '[') {
            line = eol ? eol + 1 : NULL;
            continue;
        }

        if (line[0] == 'F' && line[1] == ' ') {
            /* Parse fact: F <id> <T/F/U> <confidence> <is_derived> <desc> */
            uint32_t var_id = 0;
            char truth = 'U';
            float conf = 0.0f;
            int is_derived = 0;

            if (sscanf(line + 2, "%u %c %f %d", &var_id, &truth, &conf, &is_derived) >= 2) {
                if (var_id < QREASON_MAX_VARIABLES) {
                    int8_t new_truth = parse_truth_char(truth);

                    /* Conflict resolution */
                    if (ctx->kb.facts[var_id] != 0 && ctx->kb.facts[var_id] != new_truth) {
                        if (conf > ctx->kb.confidences[var_id]) {
                            ctx->kb.facts[var_id] = new_truth;
                            ctx->kb.confidences[var_id] = conf;
                        }
                        conflicts++;
                    } else {
                        ctx->kb.facts[var_id] = new_truth;
                        ctx->kb.confidences[var_id] = conf;
                    }

                    if (var_id + 1 > ctx->kb.n_variables) {
                        ctx->kb.n_variables = var_id + 1;
                    }

                    facts_imported++;
                }
            }
        } else if (line[0] == 'R' && line[1] == ' ') {
            /* Parse rule: R <ant...> -> <cons> <confidence> <desc> */
            if (ctx->kb.n_rules < QREASON_MAX_RULES) {
                qreason_rule_t* rule = &ctx->kb.rules[ctx->kb.n_rules];
                memset(rule, 0, sizeof(*rule));

                char* p = line + 2;
                uint32_t ant_count = 0;

                /* Read antecedents until "->" */
                while (p && *p && ant_count < 8) {
                    /* Skip whitespace */
                    while (*p == ' ') p++;
                    if (*p == '-' && *(p + 1) == '>') break;
                    if (*p == '\0') break;

                    uint32_t val = 0;
                    if (sscanf(p, "%u", &val) == 1) {
                        rule->antecedents[ant_count++] = val;
                    }

                    /* Advance past this number */
                    while (*p && *p != ' ') p++;
                }

                rule->n_antecedents = ant_count;

                /* Find "->" */
                char* arrow = strstr(p, "->");
                if (arrow) {
                    arrow += 2;
                    while (*arrow == ' ') arrow++;

                    float conf = 0.0f;
                    if (sscanf(arrow, "%u %f", &rule->consequent, &conf) >= 1) {
                        rule->confidence = conf;
                        ctx->kb.n_rules++;
                        rules_imported++;
                    }
                }
            }
        }

        line = eol ? eol + 1 : NULL;
    }

    nimcp_free(text);
    text = NULL;

    if (result) {
        result->num_facts_imported = facts_imported;
        result->num_rules_imported = rules_imported;
        result->num_conflicts = conflicts;
        result->bytes_read = size;
    }

    return 0;
}

/**
 * @brief Detect if buffer is text format
 *
 * WHAT: Check if buffer starts with '#' (text header comment)
 * WHY:  Auto-detect format for import
 */
static bool is_text_format(const uint8_t* buffer, size_t size)
{
    if (!buffer || size == 0) return false;
    return buffer[0] == '#';
}

int reasoning_kb_import_from_buffer(
    void* kb_target,
    const uint8_t* buffer,
    size_t size,
    kb_import_result_t* result)
{
    /* Guard clauses */
    if (!kb_target) return -1;
    if (!buffer) return -1;
    if (size == 0) return -1;

    qreason_t ctx = (qreason_t)kb_target;

    /* Auto-detect format */
    if (is_text_format(buffer, size)) {
        return import_text(ctx, buffer, size, result);
    }

    return import_binary(ctx, buffer, size, result);
}

/*=============================================================================
 * FILE I/O
 *===========================================================================*/

int reasoning_kb_save_to_file(
    const void* kb_source,
    const char* filepath,
    const kb_persistence_config_t* config,
    kb_export_result_t* result)
{
    /* Guard clauses */
    if (!kb_source) return -1;
    if (!filepath) return -1;

    uint8_t* buffer = NULL;
    size_t size = 0;

    /* Export to buffer first */
    if (reasoning_kb_export_to_buffer(kb_source, config, &buffer, &size) != 0) {
        return -1;
    }

    /* Write to file */
    FILE* f = fopen(filepath, "wb");
    if (!f) {
        nimcp_free(buffer);
        buffer = NULL;
        return -1;
    }

    size_t written = fwrite(buffer, 1, size, f);
    fclose(f);

    if (written != size) {
        nimcp_free(buffer);
        buffer = NULL;
        return -1;
    }

    /* Fill result */
    if (result) {
        /* Extract header info */
        kb_persistence_config_t actual;
        if (config) {
            actual = *config;
        } else {
            actual = reasoning_kb_default_config();
        }

        if (actual.format == KB_FORMAT_BINARY && size >= sizeof(kb_header_t)) {
            const kb_header_t* header = (const kb_header_t*)buffer;
            result->num_facts_exported = header->num_facts;
            result->num_rules_exported = header->num_rules;
            result->checksum = header->checksum;
        } else {
            result->num_facts_exported = 0;
            result->num_rules_exported = 0;
            result->checksum = 0;
        }
        result->bytes_written = written;
    }

    nimcp_free(buffer);
    buffer = NULL;
    return 0;
}

int reasoning_kb_load_from_file(
    void* kb_target,
    const char* filepath,
    kb_import_result_t* result)
{
    /* Guard clauses */
    if (!kb_target) return -1;
    if (!filepath) return -1;

    /* Open file */
    FILE* f = fopen(filepath, "rb");
    if (!f) return -1;

    /* Get file size */
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return -1;
    }

    long file_size = ftell(f);
    if (file_size <= 0) {
        fclose(f);
        return -1;
    }

    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return -1;
    }

    /* Read into buffer */
    uint8_t* buffer = (uint8_t*)nimcp_malloc((size_t)file_size);
    if (!buffer) {
        fclose(f);
        return -1;
    }

    size_t bytes_read = fread(buffer, 1, (size_t)file_size, f);
    fclose(f);

    if (bytes_read != (size_t)file_size) {
        nimcp_free(buffer);
        buffer = NULL;
        return -1;
    }

    /* Import from buffer */
    int rc = reasoning_kb_import_from_buffer(kb_target, buffer, bytes_read, result);
    nimcp_free(buffer);
    buffer = NULL;
    return rc;
}

/*=============================================================================
 * VALIDATION AND CHECKSUM
 *===========================================================================*/

int reasoning_kb_validate_buffer(
    const uint8_t* buffer,
    size_t size)
{
    /* Guard clauses */
    if (!buffer) return -1;
    if (size < sizeof(kb_header_t)) return -1;

    const kb_header_t* header = (const kb_header_t*)buffer;

    /* Check magic number */
    if (header->magic != KB_PERSISTENCE_MAGIC) return -1;

    /* Check version */
    if (header->version != KB_PERSISTENCE_VERSION) return -1;

    /* Check buffer is large enough for declared content */
    size_t expected_data = (size_t)header->num_facts * sizeof(kb_fact_entry_t)
                         + (size_t)header->num_rules * sizeof(kb_rule_entry_t);
    size_t expected_total = sizeof(kb_header_t) + expected_data;
    if (size < expected_total) return -1;

    /* Recalculate checksum */
    const uint8_t* data_start = buffer + sizeof(kb_header_t);
    uint32_t computed = fnv1a_hash(data_start, expected_data);
    if (computed != header->checksum) return -1;

    return 0;
}

uint32_t reasoning_kb_get_checksum(
    const uint8_t* buffer,
    size_t size)
{
    if (!buffer || size <= sizeof(kb_header_t)) return 0;

    const uint8_t* data_start = buffer + sizeof(kb_header_t);
    size_t data_len = size - sizeof(kb_header_t);

    return fnv1a_hash(data_start, data_len);
}
