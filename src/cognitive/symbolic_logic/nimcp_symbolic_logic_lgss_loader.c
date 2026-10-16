/**
 * @file nimcp_symbolic_logic_lgss_loader.c
 * @brief LGSS Component A1: JSON loader implementation for Safety Rules
 *
 * WHAT: Parse LGSS JSON format and load safety rules into KB
 * WHY:  Enable declarative safety rule definition in human-readable format
 * HOW:  Simple JSON parser, schema validation, conversion to safety_rule_t
 *
 * JSON PARSER NOTE:
 * This implementation uses a simple hand-written JSON parser to avoid
 * external dependencies. For production use with complex JSON requirements,
 * consider using a vetted JSON library like cJSON or jansson.
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 * @version 1.0.0
 */

#include "cognitive/symbolic_logic/nimcp_symbolic_logic_lgss_loader.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "core/brain/nimcp_brain_kg.h"           /* W7: KG rule emit */
#include "core/brain/nimcp_brain_internal.h"
#include "utils/time/nimcp_time.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>

#define LOG_MODULE "lgss_loader"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(symbolic_logic_lgss_loader, MESH_ADAPTER_CATEGORY_COGNITIVE)



//=============================================================================
// Simple JSON Parser Types
//=============================================================================

typedef enum {
    JSON_NULL,
    JSON_BOOL,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT
} json_type_t;

typedef struct json_value json_value_t;
typedef struct json_pair json_pair_t;

struct json_pair {
    char* key;
    json_value_t* value;
    json_pair_t* next;
};

struct json_value {
    json_type_t type;
    union {
        bool bool_val;
        double num_val;
        char* str_val;
        struct {
            json_value_t** items;
            size_t count;
        } array;
        json_pair_t* object;
    };
};

//=============================================================================
// JSON Parser Context
//=============================================================================

typedef struct {
    const char* json;
    size_t pos;
    size_t len;
    int line;
    int column;
    char error[NIMCP_ERROR_BUFFER_SIZE];
} json_ctx_t;

//=============================================================================
// JSON Parser Forward Declarations
//=============================================================================

static json_value_t* json_parse_value(json_ctx_t* ctx);
static void json_free_value(json_value_t* val);
static json_value_t* json_object_get(json_value_t* obj, const char* key);
static const char* json_get_string(json_value_t* val);
static double json_get_number(json_value_t* val);
static bool json_get_bool(json_value_t* val);
static size_t json_array_length(json_value_t* arr);
static json_value_t* json_array_get(json_value_t* arr, size_t index);

//=============================================================================
// JSON Parser Implementation
//=============================================================================

static void json_skip_whitespace(json_ctx_t* ctx) {
    while (ctx->pos < ctx->len) {
        char c = ctx->json[ctx->pos];
        if (c == ' ' || c == '\t' || c == '\r') {
            ctx->pos++;
            ctx->column++;
        } else if (c == '\n') {
            ctx->pos++;
            ctx->line++;
            ctx->column = 1;
        } else {
            break;
        }
    }
}

static char json_peek(json_ctx_t* ctx) {
    json_skip_whitespace(ctx);
    if (ctx->pos >= ctx->len) return '\0';
    return ctx->json[ctx->pos];
}

static char json_next(json_ctx_t* ctx) {
    if (ctx->pos >= ctx->len) return '\0';
    char c = ctx->json[ctx->pos++];
    if (c == '\n') {
        ctx->line++;
        ctx->column = 1;
    } else {
        ctx->column++;
    }
    return c;
}

static bool json_expect(json_ctx_t* ctx, char expected) {
    json_skip_whitespace(ctx);
    if (ctx->pos >= ctx->len || ctx->json[ctx->pos] != expected) {
        snprintf(ctx->error, sizeof(ctx->error),
                 "Expected '%c' at line %d, column %d", expected, ctx->line, ctx->column);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "json_expect: capacity exceeded");
        return false;
    }
    json_next(ctx);
    return true;
}

static json_value_t* json_alloc_value(json_type_t type) {
    json_value_t* val = (json_value_t*)nimcp_calloc(1, sizeof(json_value_t));
    if (val) val->type = type;
    return val;
}

static json_value_t* json_parse_null(json_ctx_t* ctx) {
    if (strncmp(ctx->json + ctx->pos, "null", 4) == 0) {
        ctx->pos += 4;
        ctx->column += 4;
        return json_alloc_value(JSON_NULL);
    }
    snprintf(ctx->error, sizeof(ctx->error), "Invalid null at line %d", ctx->line);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "json_parse_null: validation failed");
    return NULL;
}

static json_value_t* json_parse_bool(json_ctx_t* ctx) {
    json_value_t* val = json_alloc_value(JSON_BOOL);
    if (!val) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "val is NULL");

        return NULL;

    }

    if (strncmp(ctx->json + ctx->pos, "true", 4) == 0) {
        ctx->pos += 4;
        ctx->column += 4;
        val->bool_val = true;
    } else if (strncmp(ctx->json + ctx->pos, "false", 5) == 0) {
        ctx->pos += 5;
        ctx->column += 5;
        val->bool_val = false;
    } else {
        nimcp_free(val);
        val = NULL;
        snprintf(ctx->error, sizeof(ctx->error), "Invalid boolean at line %d", ctx->line);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "json_parse_bool: operation failed");
        return NULL;
    }
    return val;
}

static json_value_t* json_parse_number(json_ctx_t* ctx) {
    char* end;
    double num = strtod(ctx->json + ctx->pos, &end);
    if (end == ctx->json + ctx->pos) {
        snprintf(ctx->error, sizeof(ctx->error), "Invalid number at line %d", ctx->line);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "json_parse_number: validation failed");
        return NULL;
    }

    json_value_t* val = json_alloc_value(JSON_NUMBER);
    if (!val) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "val is NULL");

        return NULL;

    }

    val->num_val = num;
    size_t len = (size_t)(end - (ctx->json + ctx->pos));
    ctx->pos += len;
    ctx->column += (int)len;
    return val;
}

static json_value_t* json_parse_string(json_ctx_t* ctx) {
    if (!json_expect(ctx, '"')) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "json_parse_string: json_expect is NULL");
        return NULL;
    }

    size_t start = ctx->pos;
    size_t esc_count = 0;

    // Find end of string
    while (ctx->pos < ctx->len) {
        char c = ctx->json[ctx->pos];
        if (c == '"') break;
        if (c == '\\') {
            ctx->pos++;
            esc_count++;
            if (ctx->pos < ctx->len) ctx->pos++;
        } else {
            ctx->pos++;
        }
    }

    if (ctx->pos >= ctx->len) {
        snprintf(ctx->error, sizeof(ctx->error), "Unterminated string at line %d", ctx->line);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "json_parse_string: capacity exceeded");
        return NULL;
    }

    size_t str_len = ctx->pos - start;
    json_value_t* val = json_alloc_value(JSON_STRING);
    if (!val) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "val is NULL");

        return NULL;

    }

    val->str_val = (char*)nimcp_malloc(str_len + 1);
    if (!val->str_val) {
        nimcp_free(val);
        val = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "json_parse_string: val->str_val is NULL");
        return NULL;
    }

    // Copy and unescape
    size_t src = start;
    size_t dst = 0;
    while (src < ctx->pos) {
        if (ctx->json[src] == '\\' && src + 1 < ctx->pos) {
            src++;
            switch (ctx->json[src]) {
                case 'n': val->str_val[dst++] = '\n'; break;
                case 't': val->str_val[dst++] = '\t'; break;
                case 'r': val->str_val[dst++] = '\r'; break;
                case '"': val->str_val[dst++] = '"'; break;
                case '\\': val->str_val[dst++] = '\\'; break;
                default: val->str_val[dst++] = ctx->json[src]; break;
            }
            src++;
        } else {
            val->str_val[dst++] = ctx->json[src++];
        }
    }
    val->str_val[dst] = '\0';

    ctx->pos++;  // Skip closing quote
    ctx->column += (int)str_len + 2;
    return val;
}

static json_value_t* json_parse_array(json_ctx_t* ctx) {
    if (!json_expect(ctx, '[')) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "json_parse_array: json_expect is NULL");
        return NULL;
    }

    json_value_t* val = json_alloc_value(JSON_ARRAY);
    if (!val) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "val is NULL");

        return NULL;

    }

    val->array.items = NULL;
    val->array.count = 0;

    size_t capacity = 8;
    val->array.items = (json_value_t**)nimcp_calloc(capacity, sizeof(json_value_t*));
    if (!val->array.items) {
        nimcp_free(val);
        val = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "json_parse_array: val->array is NULL");
        return NULL;
    }

    if (json_peek(ctx) == ']') {
        json_next(ctx);
        return val;
    }

    while (1) {
        json_value_t* item = json_parse_value(ctx);
        if (!item) {
            json_free_value(val);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "json_parse_array: item is NULL");
            return NULL;
        }

        if (val->array.count >= capacity) {
            capacity *= 2;
            json_value_t** new_items = (json_value_t**)nimcp_realloc(
                val->array.items, capacity * sizeof(json_value_t*));
            if (!new_items) {
                json_free_value(item);
                json_free_value(val);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "json_parse_array: new_items is NULL");
                return NULL;
            }
            val->array.items = new_items;
        }
        val->array.items[val->array.count++] = item;

        json_skip_whitespace(ctx);
        char c = json_peek(ctx);
        if (c == ']') {
            json_next(ctx);
            break;
        }
        if (!json_expect(ctx, ',')) {
            json_free_value(val);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "json_parse_array: json_expect is NULL");
            return NULL;
        }
    }

    return val;
}

static json_value_t* json_parse_object(json_ctx_t* ctx) {
    if (!json_expect(ctx, '{')) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "json_parse_object: json_expect is NULL");
        return NULL;
    }

    json_value_t* val = json_alloc_value(JSON_OBJECT);
    if (!val) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "val is NULL");

        return NULL;

    }

    val->object = NULL;

    if (json_peek(ctx) == '}') {
        json_next(ctx);
        return val;
    }

    json_pair_t* tail = NULL;

    while (1) {
        json_skip_whitespace(ctx);

        // Parse key
        json_value_t* key_val = json_parse_string(ctx);
        if (!key_val) {
            json_free_value(val);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "json_parse_object: key_val is NULL");
            return NULL;
        }

        if (!json_expect(ctx, ':')) {
            json_free_value(key_val);
            json_free_value(val);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "json_parse_object: json_expect is NULL");
            return NULL;
        }

        // Parse value
        json_value_t* item = json_parse_value(ctx);
        if (!item) {
            json_free_value(key_val);
            json_free_value(val);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "json_parse_object: item is NULL");
            return NULL;
        }

        // Create pair
        json_pair_t* pair = (json_pair_t*)nimcp_malloc(sizeof(json_pair_t));
        if (!pair) {
            json_free_value(key_val);
            json_free_value(item);
            json_free_value(val);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "json_parse_object: pair is NULL");
            return NULL;
        }
        pair->key = key_val->str_val;
        key_val->str_val = NULL;
        json_free_value(key_val);
        pair->value = item;
        pair->next = NULL;

        if (!val->object) {
            val->object = pair;
        } else {
            tail->next = pair;
        }
        tail = pair;

        json_skip_whitespace(ctx);
        char c = json_peek(ctx);
        if (c == '}') {
            json_next(ctx);
            break;
        }
        if (!json_expect(ctx, ',')) {
            json_free_value(val);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "json_parse_object: json_expect is NULL");
            return NULL;
        }
    }

    return val;
}

static json_value_t* json_parse_value(json_ctx_t* ctx) {
    json_skip_whitespace(ctx);
    char c = json_peek(ctx);

    if (c == 'n') return json_parse_null(ctx);
    if (c == 't' || c == 'f') return json_parse_bool(ctx);
    if (c == '"') return json_parse_string(ctx);
    if (c == '[') return json_parse_array(ctx);
    if (c == '{') return json_parse_object(ctx);
    if (c == '-' || isdigit(c)) return json_parse_number(ctx);

    snprintf(ctx->error, sizeof(ctx->error),
             "Unexpected character '%c' at line %d, column %d", c, ctx->line, ctx->column);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "json_parse_value: validation failed");
    return NULL;
}

static void json_free_value(json_value_t* val) {
    if (!val) return;

    switch (val->type) {
        case JSON_STRING:
            if (val->str_val) nimcp_free(val->str_val);
            break;
        case JSON_ARRAY:
            for (size_t i = 0; i < val->array.count; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && val->array.count > 256) {
                    symbolic_logic_lgss_loader_heartbeat("symbolic_log_loop",
                                     (float)(i + 1) / (float)val->array.count);
                }

                json_free_value(val->array.items[i]);
            }
            if (val->array.items) nimcp_free(val->array.items);
            break;
        case JSON_OBJECT: {
            json_pair_t* pair = val->object;
            while (pair) {
                json_pair_t* next = pair->next;
                if (pair->key) nimcp_free(pair->key);
                json_free_value(pair->value);
                nimcp_free(pair);
                pair = NULL;
                pair = next;
            }
            break;
        }
        default:
            break;
    }
    nimcp_free(val);
    val = NULL;
}

static json_value_t* json_parse(const char* json, size_t len, char* error, size_t error_size) {
    if (!json) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "json is NULL");

        return NULL;

    }
    if (len == 0) len = strlen(json);

    json_ctx_t ctx = {
        .json = json,
        .pos = 0,
        .len = len,
        .line = 1,
        .column = 1,
        .error = {0}
    };

    json_value_t* val = json_parse_value(&ctx);
    if (!val && error && error_size > 0) {
        strncpy(error, ctx.error, error_size - 1);
        error[error_size - 1] = '\0';
    }
    return val;
}

static json_value_t* json_object_get(json_value_t* obj, const char* key) {
    /* P2-COG-16: Returning NULL for invalid args or missing key is normal lookup behavior */
    if (!obj || obj->type != JSON_OBJECT || !key) {
        return NULL;
    }

    for (json_pair_t* pair = obj->object; pair; pair = pair->next) {
        if (strcmp(pair->key, key) == 0) {
            return pair->value;
        }
    }
    /* Key not found is normal lookup behavior, not an error */
    return NULL;
}

static const char* json_get_string(json_value_t* val) {
    /* P2-COG-17: Type mismatch or NULL is normal, not an error */
    if (!val || val->type != JSON_STRING) {
        return NULL;
    }
    return val->str_val;
}

static double json_get_number(json_value_t* val) {
    if (!val || val->type != JSON_NUMBER) return 0.0;
    return val->num_val;
}

static bool json_get_bool(json_value_t* val) {
    /* P2-COG-18: Type mismatch or NULL is normal, not an error */
    if (!val || val->type != JSON_BOOL) {
        return false;
    }
    return val->bool_val;
}

static size_t json_array_length(json_value_t* arr) {
    if (!arr || arr->type != JSON_ARRAY) return 0;
    return arr->array.count;
}

static json_value_t* json_array_get(json_value_t* arr, size_t index) {
    /* P2-COG-19: OOB or NULL is normal lookup behavior, not an error */
    if (!arr || arr->type != JSON_ARRAY || index >= arr->array.count) {
        return NULL;
    }
    return arr->array.items[index];
}

//=============================================================================
// LGSS Enum Parsing
//=============================================================================

/**
 * @brief Case-insensitive string comparison helper
 */
static int lgss_strcasecmp(const char* a, const char* b) {
    while (*a && *b) {
        int ca = tolower((unsigned char)*a);
        int cb = tolower((unsigned char)*b);
        if (ca != cb) return ca - cb;
        a++;
        b++;
    }
    return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}

bool symbolic_logic_lgss_parse_domain(const char* domain_str, safety_domain_t* domain_out) {
    if (!domain_str || !domain_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "symbolic_logic_lgss_parse_domain: required parameter is NULL (domain_str, domain_out)");
        return false;
    }

    if (lgss_strcasecmp(domain_str, "HUMAN_HARM") == 0) { *domain_out = SAFETY_DOMAIN_HUMAN_HARM; return true; }
    if (lgss_strcasecmp(domain_str, "BIO") == 0) { *domain_out = SAFETY_DOMAIN_BIO; return true; }
    if (lgss_strcasecmp(domain_str, "CYBER") == 0) { *domain_out = SAFETY_DOMAIN_CYBER; return true; }
    if (lgss_strcasecmp(domain_str, "WEAPONS") == 0) { *domain_out = SAFETY_DOMAIN_WEAPONS; return true; }
    if (lgss_strcasecmp(domain_str, "INFRASTRUCTURE") == 0) { *domain_out = SAFETY_DOMAIN_INFRASTRUCTURE; return true; }
    if (lgss_strcasecmp(domain_str, "REPLICATION") == 0) { *domain_out = SAFETY_DOMAIN_REPLICATION; return true; }
    if (lgss_strcasecmp(domain_str, "GOVERNANCE") == 0) { *domain_out = SAFETY_DOMAIN_GOVERNANCE; return true; }

    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_lgss_loader_heartbeat("symbolic_log_symbolic_logic_lgss_", 0.0f);


    /* Unrecognized domain string is validation rejection, not an error */
    return false;
}

bool symbolic_logic_lgss_parse_severity(const char* severity_str, safety_severity_t* severity_out) {
    if (!severity_str || !severity_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "symbolic_logic_lgss_parse_severity: required parameter is NULL (severity_str, severity_out)");
        return false;
    }

    if (lgss_strcasecmp(severity_str, "CRITICAL") == 0) { *severity_out = SAFETY_SEVERITY_CRITICAL; return true; }
    if (lgss_strcasecmp(severity_str, "HIGH") == 0) { *severity_out = SAFETY_SEVERITY_HIGH; return true; }
    if (lgss_strcasecmp(severity_str, "MEDIUM") == 0) { *severity_out = SAFETY_SEVERITY_MEDIUM; return true; }
    if (lgss_strcasecmp(severity_str, "LOW") == 0) { *severity_out = SAFETY_SEVERITY_LOW; return true; }
    if (lgss_strcasecmp(severity_str, "INFO") == 0) { *severity_out = SAFETY_SEVERITY_INFO; return true; }

    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_lgss_loader_heartbeat("symbolic_log_symbolic_logic_lgss_", 0.0f);


    /* Unrecognized severity string is validation rejection, not an error */
    return false;
}

bool symbolic_logic_lgss_parse_action(const char* action_str, safety_action_t* action_out) {
    if (!action_str || !action_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "symbolic_logic_lgss_parse_action: required parameter is NULL (action_str, action_out)");
        return false;
    }

    if (lgss_strcasecmp(action_str, "ALLOW") == 0) { *action_out = SAFETY_ACTION_ALLOW; return true; }
    if (lgss_strcasecmp(action_str, "DENY") == 0) { *action_out = SAFETY_ACTION_DENY; return true; }
    if (lgss_strcasecmp(action_str, "ESCALATE") == 0) { *action_out = SAFETY_ACTION_ESCALATE; return true; }
    if (lgss_strcasecmp(action_str, "LOG") == 0) { *action_out = SAFETY_ACTION_LOG; return true; }
    if (lgss_strcasecmp(action_str, "WARN") == 0) { *action_out = SAFETY_ACTION_WARN; return true; }

    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_lgss_loader_heartbeat("symbolic_log_symbolic_logic_lgss_", 0.0f);


    /* Unrecognized action string is validation rejection, not an error */
    return false;
}

bool symbolic_logic_lgss_parse_operator(const char* op_str, safety_condition_op_t* op_out) {
    if (!op_str || !op_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "symbolic_logic_lgss_parse_operator: required parameter is NULL (op_str, op_out)");
        return false;
    }

    if (lgss_strcasecmp(op_str, "EQ") == 0 || strcmp(op_str, "==") == 0) { *op_out = SAFETY_COND_OP_EQ; return true; }
    if (lgss_strcasecmp(op_str, "NEQ") == 0 || strcmp(op_str, "!=") == 0) { *op_out = SAFETY_COND_OP_NEQ; return true; }
    if (lgss_strcasecmp(op_str, "GT") == 0 || strcmp(op_str, ">") == 0) { *op_out = SAFETY_COND_OP_GT; return true; }
    if (lgss_strcasecmp(op_str, "LT") == 0 || strcmp(op_str, "<") == 0) { *op_out = SAFETY_COND_OP_LT; return true; }
    if (lgss_strcasecmp(op_str, "GTE") == 0 || strcmp(op_str, ">=") == 0) { *op_out = SAFETY_COND_OP_GTE; return true; }
    if (lgss_strcasecmp(op_str, "LTE") == 0 || strcmp(op_str, "<=") == 0) { *op_out = SAFETY_COND_OP_LTE; return true; }
    if (lgss_strcasecmp(op_str, "IN") == 0) { *op_out = SAFETY_COND_OP_IN; return true; }
    if (lgss_strcasecmp(op_str, "NOT_IN") == 0) { *op_out = SAFETY_COND_OP_NOT_IN; return true; }
    if (lgss_strcasecmp(op_str, "CONTAINS") == 0) { *op_out = SAFETY_COND_OP_CONTAINS; return true; }
    if (lgss_strcasecmp(op_str, "MATCHES") == 0) { *op_out = SAFETY_COND_OP_MATCHES; return true; }

    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_lgss_loader_heartbeat("symbolic_log_symbolic_logic_lgss_", 0.0f);


    /* Unrecognized operator string is validation rejection, not an error */
    return false;
}

//=============================================================================
// Error String
//=============================================================================

const char* symbolic_logic_lgss_error_string(lgss_error_t error_code) {
    switch (error_code) {
        case LGSS_OK: return "Success";
        case LGSS_ERROR_NULL_ARG: return "NULL argument provided";
        case LGSS_ERROR_FILE_NOT_FOUND: return "File not found";
        case LGSS_ERROR_FILE_READ: return "File read error";
        case LGSS_ERROR_FILE_TOO_LARGE: return "File exceeds maximum size";
        case LGSS_ERROR_INVALID_JSON: return "Invalid JSON syntax";
        case LGSS_ERROR_SCHEMA_MISMATCH: return "Schema validation failed";
        case LGSS_ERROR_UNSUPPORTED_VERSION: return "Unsupported schema version";
        case LGSS_ERROR_MISSING_FIELD: return "Required field missing";
        case LGSS_ERROR_INVALID_VALUE: return "Invalid field value";
        case LGSS_ERROR_MEMORY: return "Memory allocation failed";
        case LGSS_ERROR_KB_FULL: return "Safety KB is full";
        case LGSS_ERROR_KB_LOCKED: return "Safety KB is locked";
        default: return "Unknown error";
    }
}

void symbolic_logic_lgss_init_result(lgss_load_result_t* result) {
    if (!result) return;
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_lgss_loader_heartbeat("symbolic_log_symbolic_logic_lgss_", 0.0f);


    memset(result, 0, sizeof(lgss_load_result_t));
    result->error_code = LGSS_OK;
}

//=============================================================================
// Rule Parsing
//=============================================================================

/**
 * @brief Parse a single condition from JSON
 */
static lgss_error_t parse_condition(json_value_t* cond_obj, safety_condition_t* cond_out) {
    if (!cond_obj || cond_obj->type != JSON_OBJECT) {
        return LGSS_ERROR_INVALID_JSON;
    }

    memset(cond_out, 0, sizeof(safety_condition_t));

    // field (required)
    json_value_t* field_val = json_object_get(cond_obj, "field");
    if (!field_val) return LGSS_ERROR_MISSING_FIELD;
    const char* field_str = json_get_string(field_val);
    if (!field_str) return LGSS_ERROR_INVALID_VALUE;
    strncpy(cond_out->field, field_str, sizeof(cond_out->field) - 1);

    // operator (required) - accept "op" as v0.2 alias
    json_value_t* op_val = json_object_get(cond_obj, "operator");
    if (!op_val) op_val = json_object_get(cond_obj, "op");
    if (!op_val) return LGSS_ERROR_MISSING_FIELD;
    const char* op_str = json_get_string(op_val);
    if (!op_str || !symbolic_logic_lgss_parse_operator(op_str, &cond_out->op)) {
        return LGSS_ERROR_INVALID_VALUE;
    }

    // value (required)
    json_value_t* value_val = json_object_get(cond_obj, "value");
    if (!value_val) return LGSS_ERROR_MISSING_FIELD;

    if (value_val->type == JSON_STRING) {
        strncpy(cond_out->value, json_get_string(value_val), sizeof(cond_out->value) - 1);
    } else if (value_val->type == JSON_NUMBER) {
        cond_out->numeric_value = (float)json_get_number(value_val);
        snprintf(cond_out->value, sizeof(cond_out->value), "%g", cond_out->numeric_value);
    } else if (value_val->type == JSON_ARRAY) {
        // Convert JSON array to comma-separated string for IN/NOT_IN operators
        size_t arr_len = json_array_length(value_val);
        size_t offset = 0;
        cond_out->value[0] = '\0';
        for (size_t vi = 0; vi < arr_len && offset < sizeof(cond_out->value) - 1; vi++) {
            json_value_t* item = json_array_get(value_val, vi);
            const char* item_str = json_get_string(item);
            if (item_str) {
                if (offset > 0 && offset < sizeof(cond_out->value) - 1) {
                    cond_out->value[offset++] = ',';
                }
                size_t slen = strlen(item_str);
                if (offset + slen >= sizeof(cond_out->value)) {
                    slen = sizeof(cond_out->value) - offset - 1;
                }
                memcpy(cond_out->value + offset, item_str, slen);
                offset += slen;
            }
        }
        cond_out->value[offset] = '\0';
    } else {
        return LGSS_ERROR_INVALID_VALUE;
    }

    // negated (optional, default false)
    json_value_t* neg_val = json_object_get(cond_obj, "negated");
    if (neg_val) {
        cond_out->is_negated = json_get_bool(neg_val);
    }

    return LGSS_OK;
}

lgss_error_t symbolic_logic_lgss_parse_rule(
    const char* rule_json,
    safety_rule_t* rule_out,
    char* error_msg,
    size_t error_msg_size)
{
    if (!rule_json || !rule_out) {
        if (error_msg) snprintf(error_msg, error_msg_size, "NULL argument");
        return LGSS_ERROR_NULL_ARG;
    }

    // Parse JSON
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_lgss_loader_heartbeat("symbolic_log_symbolic_logic_lgss_", 0.0f);


    char parse_error[NIMCP_ERROR_BUFFER_SIZE];
    json_value_t* root = json_parse(rule_json, 0, parse_error, sizeof(parse_error));
    if (!root) {
        if (error_msg) snprintf(error_msg, error_msg_size, "JSON parse error: %s", parse_error);
        return LGSS_ERROR_INVALID_JSON;
    }

    if (root->type != JSON_OBJECT) {
        json_free_value(root);
        if (error_msg) snprintf(error_msg, error_msg_size, "Rule must be a JSON object");
        return LGSS_ERROR_INVALID_JSON;
    }

    // Initialize rule
    symbolic_logic_safety_init_rule(rule_out);

    // name (required)
    json_value_t* name_val = json_object_get(root, "name");
    if (!name_val) {
        json_free_value(root);
        if (error_msg) snprintf(error_msg, error_msg_size, "Missing required field: name");
        return LGSS_ERROR_MISSING_FIELD;
    }
    const char* name_str = json_get_string(name_val);
    if (!name_str) {
        json_free_value(root);
        if (error_msg) snprintf(error_msg, error_msg_size, "Invalid name value");
        return LGSS_ERROR_INVALID_VALUE;
    }
    strncpy(rule_out->name, name_str, sizeof(rule_out->name) - 1);

    // description (optional)
    json_value_t* desc_val = json_object_get(root, "description");
    if (desc_val) {
        const char* desc_str = json_get_string(desc_val);
        if (desc_str) {
            strncpy(rule_out->description, desc_str, sizeof(rule_out->description) - 1);
        }
    }

    // domain (required)
    json_value_t* domain_val = json_object_get(root, "domain");
    if (!domain_val) {
        json_free_value(root);
        if (error_msg) snprintf(error_msg, error_msg_size, "Missing required field: domain");
        return LGSS_ERROR_MISSING_FIELD;
    }
    const char* domain_str = json_get_string(domain_val);
    if (!domain_str || !symbolic_logic_lgss_parse_domain(domain_str, &rule_out->domain)) {
        json_free_value(root);
        if (error_msg) snprintf(error_msg, error_msg_size, "Invalid domain value: %s", domain_str ? domain_str : "null");
        return LGSS_ERROR_INVALID_VALUE;
    }

    // severity (required)
    json_value_t* sev_val = json_object_get(root, "severity");
    if (!sev_val) {
        json_free_value(root);
        if (error_msg) snprintf(error_msg, error_msg_size, "Missing required field: severity");
        return LGSS_ERROR_MISSING_FIELD;
    }
    const char* sev_str = json_get_string(sev_val);
    if (!sev_str || !symbolic_logic_lgss_parse_severity(sev_str, &rule_out->severity)) {
        json_free_value(root);
        if (error_msg) snprintf(error_msg, error_msg_size, "Invalid severity value: %s", sev_str ? sev_str : "null");
        return LGSS_ERROR_INVALID_VALUE;
    }

    // action (required)
    json_value_t* action_val = json_object_get(root, "action");
    if (!action_val) {
        json_free_value(root);
        if (error_msg) snprintf(error_msg, error_msg_size, "Missing required field: action");
        return LGSS_ERROR_MISSING_FIELD;
    }
    const char* action_str = json_get_string(action_val);
    if (!action_str || !symbolic_logic_lgss_parse_action(action_str, &rule_out->action)) {
        json_free_value(root);
        if (error_msg) snprintf(error_msg, error_msg_size, "Invalid action value: %s", action_str ? action_str : "null");
        return LGSS_ERROR_INVALID_VALUE;
    }

    // priority (optional, default 0.5)
    json_value_t* priority_val = json_object_get(root, "priority");
    if (priority_val) {
        rule_out->priority = (float)json_get_number(priority_val);
    }

    // enabled (optional, default true)
    json_value_t* enabled_val = json_object_get(root, "enabled");
    if (enabled_val) {
        rule_out->enabled = json_get_bool(enabled_val);
    }

    // conditions (optional array)
    json_value_t* conditions_val = json_object_get(root, "conditions");
    if (conditions_val && conditions_val->type == JSON_ARRAY) {
        size_t num_conds = json_array_length(conditions_val);
        if (num_conds > SAFETY_MAX_CONDITIONS_PER_RULE) {
            num_conds = SAFETY_MAX_CONDITIONS_PER_RULE;
        }

        for (size_t i = 0; i < num_conds; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && num_conds > 256) {
                symbolic_logic_lgss_loader_heartbeat("symbolic_log_loop",
                                 (float)(i + 1) / (float)num_conds);
            }

            json_value_t* cond_obj = json_array_get(conditions_val, i);
            lgss_error_t err = parse_condition(cond_obj, &rule_out->conditions[i]);
            if (err != LGSS_OK) {
                json_free_value(root);
                if (error_msg) snprintf(error_msg, error_msg_size,
                                        "Failed to parse condition %zu: %s", i, symbolic_logic_lgss_error_string(err));
                return err;
            }
            rule_out->num_conditions++;
        }
    }

    json_free_value(root);
    return LGSS_OK;
}

//=============================================================================
// Schema Validation
//=============================================================================

lgss_error_t symbolic_logic_lgss_validate_schema(
    const char* json_string,
    size_t json_length,
    char* error_msg,
    size_t error_msg_size)
{
    if (!json_string) {
        if (error_msg) snprintf(error_msg, error_msg_size, "NULL JSON string");
        return LGSS_ERROR_NULL_ARG;
    }

    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_lgss_loader_heartbeat("symbolic_log_symbolic_logic_lgss_", 0.0f);


    if (json_length == 0) json_length = strlen(json_string);

    // Parse JSON
    char parse_error[NIMCP_ERROR_BUFFER_SIZE];
    json_value_t* root = json_parse(json_string, json_length, parse_error, sizeof(parse_error));
    if (!root) {
        if (error_msg) snprintf(error_msg, error_msg_size, "JSON parse error: %s", parse_error);
        return LGSS_ERROR_INVALID_JSON;
    }

    if (root->type != JSON_OBJECT) {
        json_free_value(root);
        if (error_msg) snprintf(error_msg, error_msg_size, "Root must be a JSON object");
        return LGSS_ERROR_SCHEMA_MISMATCH;
    }

    // Check version (required)
    json_value_t* version_val = json_object_get(root, "version");
    if (!version_val) {
        json_free_value(root);
        if (error_msg) snprintf(error_msg, error_msg_size, "Missing required field: version");
        return LGSS_ERROR_MISSING_FIELD;
    }
    const char* version_str = json_get_string(version_val);
    if (!version_str || (strcmp(version_str, LGSS_SCHEMA_VERSION) != 0 &&
                         strcmp(version_str, "0.2") != 0)) {
        json_free_value(root);
        if (error_msg) snprintf(error_msg, error_msg_size,
                                "Unsupported version: %s (expected %s or 0.2)",
                                version_str ? version_str : "null", LGSS_SCHEMA_VERSION);
        return LGSS_ERROR_UNSUPPORTED_VERSION;
    }
    bool is_v02 = (version_str && strcmp(version_str, "0.2") == 0);

    // Check rules (required array)
    json_value_t* rules_val = json_object_get(root, "rules");
    if (!rules_val) {
        json_free_value(root);
        if (error_msg) snprintf(error_msg, error_msg_size, "Missing required field: rules");
        return LGSS_ERROR_MISSING_FIELD;
    }
    if (rules_val->type != JSON_ARRAY) {
        json_free_value(root);
        if (error_msg) snprintf(error_msg, error_msg_size, "Field 'rules' must be an array");
        return LGSS_ERROR_SCHEMA_MISMATCH;
    }

    // Validate each rule
    size_t num_rules = json_array_length(rules_val);
    for (size_t i = 0; i < num_rules; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_rules > 256) {
            symbolic_logic_lgss_loader_heartbeat("symbolic_log_loop",
                             (float)(i + 1) / (float)num_rules);
        }

        json_value_t* rule_obj = json_array_get(rules_val, i);
        if (!rule_obj || rule_obj->type != JSON_OBJECT) {
            json_free_value(root);
            if (error_msg) snprintf(error_msg, error_msg_size, "Rule %zu must be an object", i);
            return LGSS_ERROR_SCHEMA_MISMATCH;
        }

        // Check required fields - support v0.2 alternate names
        // v1.0: name, domain, severity, action
        // v0.2: id, category, severity, on_match
        if (is_v02) {
            if (!json_object_get(rule_obj, "id") && !json_object_get(rule_obj, "name")) {
                json_free_value(root);
                if (error_msg) snprintf(error_msg, error_msg_size,
                                        "Rule %zu missing required field: id (or name)", i);
                return LGSS_ERROR_MISSING_FIELD;
            }
            if (!json_object_get(rule_obj, "category") && !json_object_get(rule_obj, "domain")) {
                json_free_value(root);
                if (error_msg) snprintf(error_msg, error_msg_size,
                                        "Rule %zu missing required field: category (or domain)", i);
                return LGSS_ERROR_MISSING_FIELD;
            }
            if (!json_object_get(rule_obj, "severity")) {
                json_free_value(root);
                if (error_msg) snprintf(error_msg, error_msg_size,
                                        "Rule %zu missing required field: severity", i);
                return LGSS_ERROR_MISSING_FIELD;
            }
            if (!json_object_get(rule_obj, "on_match") && !json_object_get(rule_obj, "action")) {
                json_free_value(root);
                if (error_msg) snprintf(error_msg, error_msg_size,
                                        "Rule %zu missing required field: on_match (or action)", i);
                return LGSS_ERROR_MISSING_FIELD;
            }
        } else {
            const char* required_fields[] = {"name", "domain", "severity", "action"};
            for (int f = 0; f < 4; f++) {
                if (!json_object_get(rule_obj, required_fields[f])) {
                    json_free_value(root);
                    if (error_msg) snprintf(error_msg, error_msg_size,
                                            "Rule %zu missing required field: %s", i, required_fields[f]);
                    return LGSS_ERROR_MISSING_FIELD;
                }
            }
        }

        // Validate domain value (accept "category" as alias)
        json_value_t* domain_val = json_object_get(rule_obj, "domain");
        if (!domain_val) domain_val = json_object_get(rule_obj, "category");
        if (domain_val) {
            safety_domain_t domain;
            if (!symbolic_logic_lgss_parse_domain(json_get_string(domain_val), &domain)) {
                json_free_value(root);
                if (error_msg) snprintf(error_msg, error_msg_size,
                                        "Rule %zu has invalid domain: %s", i, json_get_string(domain_val));
                return LGSS_ERROR_INVALID_VALUE;
            }
        }

        // Validate severity value
        json_value_t* sev_val = json_object_get(rule_obj, "severity");
        if (sev_val) {
            safety_severity_t sev;
            if (!symbolic_logic_lgss_parse_severity(json_get_string(sev_val), &sev)) {
                json_free_value(root);
                if (error_msg) snprintf(error_msg, error_msg_size,
                                        "Rule %zu has invalid severity: %s", i, json_get_string(sev_val));
                return LGSS_ERROR_INVALID_VALUE;
            }
        }

        // Validate action value (accept "on_match" as alias)
        json_value_t* action_val = json_object_get(rule_obj, "action");
        if (!action_val) action_val = json_object_get(rule_obj, "on_match");
        if (action_val) {
            safety_action_t action;
            if (!symbolic_logic_lgss_parse_action(json_get_string(action_val), &action)) {
                json_free_value(root);
                if (error_msg) snprintf(error_msg, error_msg_size,
                                        "Rule %zu has invalid action: %s", i, json_get_string(action_val));
                return LGSS_ERROR_INVALID_VALUE;
            }
        }
    }

    json_free_value(root);
    return LGSS_OK;
}

//=============================================================================
// Version API
//=============================================================================

bool symbolic_logic_lgss_get_version(
    const char* json_string,
    size_t json_length,
    char* version_out,
    size_t version_size)
{
    if (!json_string || !version_out || version_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "symbolic_logic_lgss_get_version: required parameter is NULL (json_string, version_out)");
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_lgss_loader_heartbeat("symbolic_log_symbolic_logic_lgss_", 0.0f);


    if (json_length == 0) json_length = strlen(json_string);

    char error[NIMCP_ERROR_BUFFER_SIZE];
    json_value_t* root = json_parse(json_string, json_length, error, sizeof(error));
    if (!root) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "symbolic_logic_lgss_get_version: root is NULL");
        return false;
    }

    json_value_t* version_val = json_object_get(root, "version");
    if (!version_val) {
        json_free_value(root);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "symbolic_logic_lgss_get_version: version_val is NULL");
        return false;
    }

    const char* ver_str = json_get_string(version_val);
    if (!ver_str) {
        json_free_value(root);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "symbolic_logic_lgss_get_version: ver_str is NULL");
        return false;
    }

    strncpy(version_out, ver_str, version_size - 1);
    version_out[version_size - 1] = '\0';

    json_free_value(root);
    return true;
}

//=============================================================================
// W7 KG integration helpers
//=============================================================================

static void lgss_kg_ensure_root(brain_t brain)
{
    if (!brain || !brain->internal_kg_enabled || !brain->internal_kg) return;
    uint64_t tok = brain->internal_kg_admin_token;
    brain_kg_set_access_level(brain->internal_kg, BRAIN_KG_ACCESS_ADMIN, tok);
    if (brain_kg_find_node(brain->internal_kg, "cog_symbolic_logic_lgss")
        == BRAIN_KG_INVALID_NODE) {
        brain_kg_add_node(brain->internal_kg, "cog_symbolic_logic_lgss",
                          BRAIN_KG_NODE_SECURITY,
                          "LGSS safety-rule loader (governance/safety KB)");
    }
    brain_kg_set_access_level(brain->internal_kg, BRAIN_KG_ACCESS_READ, 0);
}

static void lgss_kg_emit_load_summary(brain_t brain,
                                      uint32_t rules_loaded,
                                      uint32_t rules_failed,
                                      uint32_t rules_skipped,
                                      const char* source_name)
{
    if (!brain || !brain->internal_kg_enabled || !brain->internal_kg) return;
    uint64_t tok = brain->internal_kg_admin_token;
    brain_kg_set_access_level(brain->internal_kg, BRAIN_KG_ACCESS_ADMIN, tok);

    char node_name[BRAIN_KG_MAX_NAME_LEN];
    snprintf(node_name, sizeof(node_name),
             "cog_symbolic_logic_lgss_event_load_%llu",
             (unsigned long long)nimcp_time_monotonic_us());
    char desc[NIMCP_ERROR_BUFFER_SIZE];
    snprintf(desc, sizeof(desc),
             "LGSS load: loaded=%u failed=%u skipped=%u source=%s",
             (unsigned)rules_loaded, (unsigned)rules_failed,
             (unsigned)rules_skipped,
             source_name ? source_name : "(inline)");
    brain_kg_node_id_t nid =
        brain_kg_add_node(brain->internal_kg, node_name,
                          BRAIN_KG_NODE_SECURITY, desc);
    if (nid != BRAIN_KG_INVALID_NODE) {
        brain_kg_node_id_t root = brain_kg_find_node(brain->internal_kg,
            "cog_symbolic_logic_lgss");
        if (root != BRAIN_KG_INVALID_NODE) {
            brain_kg_add_edge(brain->internal_kg, root, nid,
                              BRAIN_KG_EDGE_PROVIDES_TO, "rules_loaded",
                              rules_failed ? 0.5f : 0.9f);
        }
    }
    brain_kg_set_access_level(brain->internal_kg, BRAIN_KG_ACCESS_READ, 0);
}

static void lgss_kg_emit_rule(brain_t brain, uint32_t rule_id,
                              const char* rule_name)
{
    if (!brain || !brain->internal_kg_enabled || !brain->internal_kg) return;
    uint64_t tok = brain->internal_kg_admin_token;
    brain_kg_set_access_level(brain->internal_kg, BRAIN_KG_ACCESS_ADMIN, tok);

    /* Stable rule node (not timestamped). */
    char node_name[BRAIN_KG_MAX_NAME_LEN];
    snprintf(node_name, sizeof(node_name), "cog_logic_rule_lgss_%u",
             (unsigned)rule_id);
    if (brain_kg_find_node(brain->internal_kg, node_name)
        == BRAIN_KG_INVALID_NODE) {
        brain_kg_node_id_t nid = brain_kg_add_node(brain->internal_kg,
            node_name, BRAIN_KG_NODE_SECURITY,
            rule_name ? rule_name : "LGSS safety rule");
        if (nid != BRAIN_KG_INVALID_NODE) {
            brain_kg_node_id_t root = brain_kg_find_node(brain->internal_kg,
                "cog_symbolic_logic_lgss");
            if (root != BRAIN_KG_INVALID_NODE) {
                brain_kg_add_edge(brain->internal_kg, root, nid,
                                  BRAIN_KG_EDGE_PROVIDES_TO, "has_rule", 0.8f);
            }
        }
    }
    brain_kg_set_access_level(brain->internal_kg, BRAIN_KG_ACCESS_READ, 0);
}

/**
 * @brief W7: Brain-aware sibling of symbolic_logic_lgss_load_file.
 *
 * Identical behaviour to the plain loader, plus: mirrors each loaded rule
 * into brain->internal_kg as a stable 'cog_logic_rule_lgss_<id>' node and
 * emits a summary event.  Brain may be NULL (falls back to plain loader).
 */
int symbolic_logic_lgss_load_file_kg(
    const char* filepath,
    safety_kb_t* kb,
    lgss_load_result_t* result,
    brain_t brain)
{
    lgss_load_result_t local_result;
    if (!result) result = &local_result;

    uint32_t prev_num_rules = kb ? kb->num_rules : 0;
    int rc = symbolic_logic_lgss_load_file(filepath, kb, result);
    if (brain && brain->internal_kg_enabled && brain->internal_kg) {
        lgss_kg_ensure_root(brain);
        /* Mirror each freshly-loaded rule. */
        if (kb && rc >= 0) {
            for (uint32_t i = prev_num_rules; i < kb->num_rules; ++i) {
                lgss_kg_emit_rule(brain, i, kb->rules[i].name);
            }
        }
        lgss_kg_emit_load_summary(brain, result->rules_loaded,
                                  result->rules_failed, result->rules_skipped,
                                  filepath);
    }
    return rc;
}

//=============================================================================
// File Loading
//=============================================================================

int symbolic_logic_lgss_load_file(
    const char* filepath,
    safety_kb_t* kb,
    lgss_load_result_t* result)
{
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_lgss_loader_heartbeat("symbolic_log_symbolic_logic_lgss_", 0.0f);


    lgss_load_result_t local_result;
    if (!result) result = &local_result;
    symbolic_logic_lgss_init_result(result);

    if (!filepath) {
        result->error_code = LGSS_ERROR_NULL_ARG;
        snprintf(result->error_message, sizeof(result->error_message), "NULL filepath");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "symbolic_logic_lgss_load_file: filepath is NULL");
        return -1;
    }
    if (!kb) {
        result->error_code = LGSS_ERROR_NULL_ARG;
        snprintf(result->error_message, sizeof(result->error_message), "NULL kb");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "symbolic_logic_lgss_load_file: kb is NULL");
        return -1;
    }
    if (kb->is_locked) {
        result->error_code = LGSS_ERROR_KB_LOCKED;
        snprintf(result->error_message, sizeof(result->error_message), "Safety KB is locked");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "symbolic_logic_lgss_load_file: validation failed");
        return -1;
    }

    // Open file
    FILE* f = fopen(filepath, "r");
    if (!f) {
        result->error_code = LGSS_ERROR_FILE_NOT_FOUND;
        snprintf(result->error_message, sizeof(result->error_message),
                 "Cannot open file: %s", filepath);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "symbolic_logic_lgss_load_file: f is NULL");
        return -1;
    }

    // Get file size
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size < 0) {
        fclose(f);
        result->error_code = LGSS_ERROR_FILE_READ;
        snprintf(result->error_message, sizeof(result->error_message), "Cannot determine file size");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "symbolic_logic_lgss_load_file: validation failed");
        return -1;
    }

    if ((size_t)file_size > LGSS_MAX_FILE_SIZE) {
        fclose(f);
        result->error_code = LGSS_ERROR_FILE_TOO_LARGE;
        snprintf(result->error_message, sizeof(result->error_message),
                 "File too large: %ld bytes (max %d)", file_size, LGSS_MAX_FILE_SIZE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "symbolic_logic_lgss_load_file: validation failed");
        return -1;
    }

    // Read file
    char* json_buffer = (char*)nimcp_malloc((size_t)file_size + 1);
    if (!json_buffer) {
        fclose(f);
        result->error_code = LGSS_ERROR_MEMORY;
        snprintf(result->error_message, sizeof(result->error_message), "Memory allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "symbolic_logic_lgss_load_file: json_buffer is NULL");
        return -1;
    }

    size_t bytes_read = fread(json_buffer, 1, (size_t)file_size, f);
    fclose(f);

    if (bytes_read != (size_t)file_size) {
        nimcp_free(json_buffer);
        json_buffer = NULL;
        result->error_code = LGSS_ERROR_FILE_READ;
        snprintf(result->error_message, sizeof(result->error_message), "File read error");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "symbolic_logic_lgss_load_file: validation failed");
        return -1;
    }
    json_buffer[file_size] = '\0';

    // Load from string
    int loaded = symbolic_logic_lgss_load_string(json_buffer, (size_t)file_size, kb, result);

    nimcp_free(json_buffer);
    json_buffer = NULL;
    return loaded;
}

int symbolic_logic_lgss_load_string(
    const char* json_string,
    size_t json_length,
    safety_kb_t* kb,
    lgss_load_result_t* result)
{
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_lgss_loader_heartbeat("symbolic_log_symbolic_logic_lgss_", 0.0f);


    lgss_load_result_t local_result;
    if (!result) result = &local_result;
    symbolic_logic_lgss_init_result(result);

    if (!json_string) {
        result->error_code = LGSS_ERROR_NULL_ARG;
        snprintf(result->error_message, sizeof(result->error_message), "NULL json_string");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "symbolic_logic_lgss_load_string: json_string is NULL");
        return -1;
    }
    if (!kb) {
        result->error_code = LGSS_ERROR_NULL_ARG;
        snprintf(result->error_message, sizeof(result->error_message), "NULL kb");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "symbolic_logic_lgss_load_string: kb is NULL");
        return -1;
    }
    if (kb->is_locked) {
        result->error_code = LGSS_ERROR_KB_LOCKED;
        snprintf(result->error_message, sizeof(result->error_message), "Safety KB is locked");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "symbolic_logic_lgss_load_string: validation failed");
        return -1;
    }

    if (json_length == 0) json_length = strlen(json_string);

    // Validate schema first
    lgss_error_t validate_err = symbolic_logic_lgss_validate_schema(
        json_string, json_length, result->error_message, sizeof(result->error_message));
    if (validate_err != LGSS_OK) {
        result->error_code = validate_err;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "symbolic_logic_lgss_load_string: validation failed");
        return -1;
    }

    // Parse JSON
    char parse_error[NIMCP_ERROR_BUFFER_SIZE];
    json_value_t* root = json_parse(json_string, json_length, parse_error, sizeof(parse_error));
    if (!root) {
        result->error_code = LGSS_ERROR_INVALID_JSON;
        snprintf(result->error_message, sizeof(result->error_message), "JSON parse error: %s", parse_error);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "symbolic_logic_lgss_load_string: root is NULL");
        return -1;
    }

    // Extract metadata
    json_value_t* version_val = json_object_get(root, "version");
    if (version_val) {
        strncpy(result->schema_version, json_get_string(version_val), sizeof(result->schema_version) - 1);
    }

    json_value_t* name_val = json_object_get(root, "name");
    if (name_val) {
        strncpy(result->file_name, json_get_string(name_val), sizeof(result->file_name) - 1);
    }

    // Load rules
    json_value_t* rules_val = json_object_get(root, "rules");
    size_t num_rules = json_array_length(rules_val);

    for (size_t i = 0; i < num_rules; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_rules > 256) {
            symbolic_logic_lgss_loader_heartbeat("symbolic_log_loop",
                             (float)(i + 1) / (float)num_rules);
        }

        json_value_t* rule_obj = json_array_get(rules_val, i);

        // Check if rule is enabled (skip disabled)
        json_value_t* enabled_val = json_object_get(rule_obj, "enabled");
        if (enabled_val && !json_get_bool(enabled_val)) {
            result->rules_skipped++;
            continue;
        }

        // Convert rule object to JSON string for parsing
        // (Simplified: we'll parse directly from the object)
        safety_rule_t rule;
        symbolic_logic_safety_init_rule(&rule);

        // Parse fields directly (with v0.2 alias support)
        json_value_t* field;

        field = json_object_get(rule_obj, "name");
        if (!field) field = json_object_get(rule_obj, "id");
        if (field) strncpy(rule.name, json_get_string(field), sizeof(rule.name) - 1);

        field = json_object_get(rule_obj, "description");
        if (field) strncpy(rule.description, json_get_string(field), sizeof(rule.description) - 1);

        field = json_object_get(rule_obj, "domain");
        if (!field) field = json_object_get(rule_obj, "category");
        if (field) symbolic_logic_lgss_parse_domain(json_get_string(field), &rule.domain);

        field = json_object_get(rule_obj, "severity");
        if (field) symbolic_logic_lgss_parse_severity(json_get_string(field), &rule.severity);

        field = json_object_get(rule_obj, "action");
        if (!field) field = json_object_get(rule_obj, "on_match");
        if (field) symbolic_logic_lgss_parse_action(json_get_string(field), &rule.action);

        field = json_object_get(rule_obj, "priority");
        if (field) rule.priority = (float)json_get_number(field);

        field = json_object_get(rule_obj, "enabled");
        rule.enabled = field ? json_get_bool(field) : true;

        // Parse conditions
        json_value_t* conditions_val = json_object_get(rule_obj, "conditions");
        if (conditions_val && conditions_val->type == JSON_ARRAY) {
            size_t num_conds = json_array_length(conditions_val);
            if (num_conds > SAFETY_MAX_CONDITIONS_PER_RULE) {
                num_conds = SAFETY_MAX_CONDITIONS_PER_RULE;
            }

            for (size_t j = 0; j < num_conds; j++) {
                /* Phase 8: Loop progress heartbeat */
                if ((j & 0xFF) == 0 && num_conds > 256) {
                    symbolic_logic_lgss_loader_heartbeat("symbolic_log_loop",
                                     (float)(j + 1) / (float)num_conds);
                }

                json_value_t* cond_obj = json_array_get(conditions_val, j);
                lgss_error_t err = parse_condition(cond_obj, &rule.conditions[j]);
                if (err == LGSS_OK) {
                    rule.num_conditions++;
                }
            }
        }

        // Add rule to KB
        uint32_t rule_id = symbolic_logic_safety_add_rule(kb, &rule);
        if (rule_id == 0) {
            result->rules_failed++;
            LOG_WARN("Failed to add rule '%s' to KB", rule.name);
        } else {
            result->rules_loaded++;
            LOG_DEBUG("Loaded rule '%s' (id=%u)", rule.name, rule_id);
        }
    }

    json_free_value(root);

    LOG_INFO("LGSS load complete: %u loaded, %u failed, %u skipped",
             result->rules_loaded, result->rules_failed, result->rules_skipped);

    return (int)result->rules_loaded;
}

//=============================================================================
// Export
//=============================================================================

int symbolic_logic_lgss_export(
    const safety_kb_t* kb,
    char* output_buffer,
    size_t buffer_size)
{
    if (!kb || !output_buffer || buffer_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "symbolic_logic_lgss_export: required parameter is NULL (kb, output_buffer)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_lgss_loader_heartbeat("symbolic_log_symbolic_logic_lgss_", 0.0f);


    char* pos = output_buffer;
    size_t remaining = buffer_size;
    int written = 0;

    // Start JSON
    written = snprintf(pos, remaining, "{\n  \"version\": \"%s\",\n  \"name\": \"Exported Safety Rules\",\n  \"rules\": [\n",
                       LGSS_SCHEMA_VERSION);
    if (written < 0 || (size_t)written >= remaining) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "symbolic_logic_lgss_export: capacity exceeded");
        return -1;
    }
    pos += written;
    remaining -= (size_t)written;

    // Write each rule
    for (uint32_t i = 0; i < kb->num_rules; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && kb->num_rules > 256) {
            symbolic_logic_lgss_loader_heartbeat("symbolic_log_loop",
                             (float)(i + 1) / (float)kb->num_rules);
        }

        const safety_rule_t* rule = &kb->rules[i];

        if (i > 0) {
            written = snprintf(pos, remaining, ",\n");
            if (written < 0 || (size_t)written >= remaining) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "symbolic_logic_lgss_export: capacity exceeded");
                return -1;
            }
            pos += written;
            remaining -= (size_t)written;
        }

        // Write rule object
        written = snprintf(pos, remaining,
                           "    {\n"
                           "      \"name\": \"%s\",\n"
                           "      \"description\": \"%s\",\n"
                           "      \"domain\": \"%s\",\n"
                           "      \"severity\": \"%s\",\n"
                           "      \"action\": \"%s\",\n"
                           "      \"priority\": %.2f,\n"
                           "      \"enabled\": %s,\n"
                           "      \"conditions\": [",
                           rule->name,
                           rule->description,
                           safety_domain_name(rule->domain),
                           safety_severity_name(rule->severity),
                           safety_action_name(rule->action),
                           rule->priority,
                           rule->enabled ? "true" : "false");
        if (written < 0 || (size_t)written >= remaining) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "symbolic_logic_lgss_export: capacity exceeded");
            return -1;
        }
        pos += written;
        remaining -= (size_t)written;

        // Write conditions
        for (uint32_t j = 0; j < rule->num_conditions; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && rule->num_conditions > 256) {
                symbolic_logic_lgss_loader_heartbeat("symbolic_log_loop",
                                 (float)(j + 1) / (float)rule->num_conditions);
            }

            const safety_condition_t* cond = &rule->conditions[j];

            if (j > 0) {
                written = snprintf(pos, remaining, ",");
                if (written < 0 || (size_t)written >= remaining) {
                    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "symbolic_logic_lgss_export: capacity exceeded");
                    return -1;
                }
                pos += written;
                remaining -= (size_t)written;
            }

            written = snprintf(pos, remaining,
                               "\n        {\n"
                               "          \"field\": \"%s\",\n"
                               "          \"operator\": \"%s\",\n"
                               "          \"value\": \"%s\"%s\n"
                               "        }",
                               cond->field,
                               safety_condition_op_name(cond->op),
                               cond->value,
                               cond->is_negated ? ",\n          \"negated\": true" : "");
            if (written < 0 || (size_t)written >= remaining) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "symbolic_logic_lgss_export: capacity exceeded");
                return -1;
            }
            pos += written;
            remaining -= (size_t)written;
        }

        // Close conditions and rule
        written = snprintf(pos, remaining, "\n      ]\n    }");
        if (written < 0 || (size_t)written >= remaining) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "symbolic_logic_lgss_export: capacity exceeded");
            return -1;
        }
        pos += written;
        remaining -= (size_t)written;
    }

    // Close JSON
    written = snprintf(pos, remaining, "\n  ]\n}\n");
    if (written < 0 || (size_t)written >= remaining) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "symbolic_logic_lgss_export: capacity exceeded");
        return -1;
    }
    pos += written;

    return (int)(pos - output_buffer);
}

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
void symbolic_logic_lgss_loader_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_symbolic_logic_lgss_loader_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training stubs
 * ============================================================================ */
int symbolic_logic_lgss_loader_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "symbolic_logic_lgss_loader_training_begin: NULL argument");
        return -1;
    }
    symbolic_logic_lgss_loader_heartbeat_instance(NULL, "symbolic_logic_lgss_loader_training_begin", 0.0f);
    return 0;
}

int symbolic_logic_lgss_loader_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "symbolic_logic_lgss_loader_training_end: NULL argument");
        return -1;
    }
    symbolic_logic_lgss_loader_heartbeat_instance(NULL, "symbolic_logic_lgss_loader_training_end", 1.0f);
    return 0;
}

int symbolic_logic_lgss_loader_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "symbolic_logic_lgss_loader_training_step: NULL argument");
        return -1;
    }
    symbolic_logic_lgss_loader_heartbeat_instance(NULL, "symbolic_logic_lgss_loader_training_step", progress);
    return 0;
}
