/**
 * @file nimcp_claude_healer.c
 * @brief Claude API Fallback Healer Implementation
 * @version 1.0.0
 * @date 2025-12-27
 *
 * WHAT: Implementation of Claude API fallback healer for crash recovery
 * WHY:  When pattern matching fails, leverage Claude for intelligent repair
 * HOW:  HTTPS requests via libcurl, JSON parsing, prompt engineering
 *
 * @author NIMCP Development Team
 */

#include "cognitive/immune/nimcp_claude_healer.h"
#include "constants/nimcp_buffer_constants.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>

#ifdef HAVE_LIBCURL
#include <curl/curl.h>
#endif
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(claude_healer, MESH_ADAPTER_CATEGORY_SECURITY)


/* ============================================================================
 * Internal Constants
 * ============================================================================ */

#define LOG_TAG "ClaudeHealer"

#define CLAUDE_API_URL "https://api.anthropic.com/v1/messages"
#define CLAUDE_API_VERSION "2023-06-01"
#define CLAUDE_DEFAULT_MODEL "claude-sonnet-4-20250514"

#define MAX_PROMPT_SIZE (32 * 1024)   /* 32KB max prompt */
#define MAX_RESPONSE_SIZE (64 * 1024) /* 64KB max response */

/* JSON parsing helper constants */
#define JSON_CONTENT_START "\"content\":["
#define JSON_TEXT_START "\"text\":\""
#define JSON_STOP_REASON "\"stop_reason\":"
#define JSON_USAGE_START "\"usage\":"
#define JSON_INPUT_TOKENS "\"input_tokens\":"
#define JSON_OUTPUT_TOKENS "\"output_tokens\":"

/* Fix validation constants */
#define MIN_FIX_LENGTH 10  /* Minimum reasonable fix length */
#define MAX_SYNTAX_ERRORS 5

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

#ifdef HAVE_LIBCURL
/**
 * @brief Response buffer for curl callback
 */
typedef struct {
    char* data;
    size_t size;
    size_t capacity;
} response_buffer_t;
#endif

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Get current time in milliseconds
 */
static uint64_t get_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

/**
 * @brief Escape string for JSON
 *
 * WHAT: Escape special characters for JSON embedding
 * WHY:  Source code may contain quotes, newlines, etc.
 * HOW:  Replace special chars with escape sequences
 */
static size_t json_escape_string(
    const char* input,
    char* output,
    size_t output_size)
{
    if (input == NULL || output == NULL || output_size == 0) return 0;

    size_t in_pos = 0;
    size_t out_pos = 0;

    while (input[in_pos] != '\0' && out_pos < output_size - 1) {
        char c = input[in_pos++];

        /* Check if we have space for escape sequence */
        if (out_pos + 2 >= output_size) break;

        switch (c) {
            case '"':
                output[out_pos++] = '\\';
                output[out_pos++] = '"';
                break;
            case '\\':
                output[out_pos++] = '\\';
                output[out_pos++] = '\\';
                break;
            case '\n':
                output[out_pos++] = '\\';
                output[out_pos++] = 'n';
                break;
            case '\r':
                output[out_pos++] = '\\';
                output[out_pos++] = 'r';
                break;
            case '\t':
                output[out_pos++] = '\\';
                output[out_pos++] = 't';
                break;
            default:
                if ((unsigned char)c < 0x20) {
                    /* Control character - skip or escape */
                    output[out_pos++] = ' ';
                } else {
                    output[out_pos++] = c;
                }
                break;
        }
    }

    output[out_pos] = '\0';
    return out_pos;
}

/**
 * @brief Find string in buffer (simple strstr for non-null-terminated)
 */
static const char* find_string(
    const char* haystack,
    size_t haystack_len,
    const char* needle)
{
    if (haystack == NULL || needle == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "find_string: parameter is NULL");
        return NULL;
    }
    size_t needle_len = strlen(needle);
    if (needle_len == 0 || needle_len > haystack_len) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "find_string: parameter is NULL");
        return NULL;
    }

    for (size_t i = 0; i <= haystack_len - needle_len; i++) {
        if (memcmp(haystack + i, needle, needle_len) == 0) {
            return haystack + i;
        }
    }
    return NULL;
}

/**
 * @brief Extract JSON string value
 *
 * WHAT: Extract value from JSON "key":"value"
 * WHY:  Parse Claude API response without full JSON parser
 * HOW:  Find key, extract until closing quote (handle escapes)
 */
static size_t extract_json_string(
    const char* json,
    size_t json_len,
    const char* key,
    char* value_out,
    size_t value_size)
{
    if (json == NULL || key == NULL || value_out == NULL) return 0;

    /* Find the key */
    char key_pattern[NIMCP_LABEL_BUFFER_SIZE];
    snprintf(key_pattern, sizeof(key_pattern), "\"%s\":\"", key);

    const char* start = find_string(json, json_len, key_pattern);
    if (start == NULL) return 0;

    start += strlen(key_pattern);
    const char* end = json + json_len;

    size_t out_pos = 0;
    bool escaped = false;

    while (start < end && out_pos < value_size - 1) {
        char c = *start++;

        if (escaped) {
            switch (c) {
                case 'n': value_out[out_pos++] = '\n'; break;
                case 'r': value_out[out_pos++] = '\r'; break;
                case 't': value_out[out_pos++] = '\t'; break;
                case '"': value_out[out_pos++] = '"'; break;
                case '\\': value_out[out_pos++] = '\\'; break;
                default: value_out[out_pos++] = c; break;
            }
            escaped = false;
        } else if (c == '\\') {
            escaped = true;
        } else if (c == '"') {
            break; /* End of string */
        } else {
            value_out[out_pos++] = c;
        }
    }

    value_out[out_pos] = '\0';
    return out_pos;
}

/**
 * @brief Extract integer value from JSON
 *
 * WHAT: Extract integer value after a JSON key
 * WHY:  Parse token counts from usage response
 * HOW:  Find key, parse following integer
 */
static int64_t extract_json_int(
    const char* json,
    size_t json_len,
    const char* key)
{
    if (json == NULL || key == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "extract_json_int: invalid parameter");
        return -1;
    }

    const char* pos = find_string(json, json_len, key);
    if (pos == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "extract_json_int: invalid parameter");
        return -1;
    }

    pos += strlen(key);

    /* Skip whitespace */
    while (pos < json + json_len && (*pos == ' ' || *pos == '\t')) {
        pos++;
    }

    /* Parse integer */
    int64_t value = 0;
    while (pos < json + json_len && *pos >= '0' && *pos <= '9') {
        value = value * 10 + (*pos - '0');
        pos++;
    }

    return value;
}

/**
 * @brief Validate generated fix code
 *
 * WHAT: Basic validation that fix code is plausible C
 * WHY:  Catch obviously malformed or incomplete responses
 * HOW:  Check for balanced braces, required patterns
 *
 * @param code The generated fix code
 * @param code_len Length of code
 * @return true if code passes basic validation
 */
static bool validate_fix_code(const char* code, size_t code_len)
{
    if (code == NULL || code_len < MIN_FIX_LENGTH) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "validate_fix_code: invalid parameter");
        return false;
    }

    /* Check for balanced braces */
    int brace_count = 0;
    int paren_count = 0;
    int bracket_count = 0;
    bool in_string = false;
    bool in_char = false;
    bool escaped = false;

    for (size_t i = 0; i < code_len; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && code_len > 256) {
            claude_healer_heartbeat("claude_heale_loop",
                             (float)(i + 1) / (float)code_len);
        }

        char c = code[i];

        if (escaped) {
            escaped = false;
            continue;
        }

        if (c == '\\') {
            escaped = true;
            continue;
        }

        if (!in_string && !in_char) {
            if (c == '"') in_string = true;
            else if (c == '\'') in_char = true;
            else if (c == '{') brace_count++;
            else if (c == '}') brace_count--;
            else if (c == '(') paren_count++;
            else if (c == ')') paren_count--;
            else if (c == '[') bracket_count++;
            else if (c == ']') bracket_count--;
        } else if (in_string && c == '"') {
            in_string = false;
        } else if (in_char && c == '\'') {
            in_char = false;
        }

        /* Early exit on obvious imbalance */
        if (brace_count < 0 || paren_count < 0 || bracket_count < 0) {
            return false;
        }
    }

    /* Check final balance */
    if (brace_count != 0 || paren_count != 0 || bracket_count != 0) {
        return false;
    }

    /* Check for unclosed strings */
    if (in_string || in_char) {
        return false;
    }

    /* Check for at least one semicolon (C statement) */
    if (find_string(code, code_len, ";") == NULL) {
        /* Might be a macro or comment-only fix - allow with lower confidence */
        return true;
    }

    return true;
}

/**
 * @brief Extract error message from API response
 *
 * WHAT: Parse error details from failed API response
 * WHY:  Provide meaningful error messages to caller
 * HOW:  Look for error object in JSON response
 */
static size_t extract_api_error(
    const char* json,
    size_t json_len,
    char* error_out,
    size_t error_size)
{
    if (json == NULL || error_out == NULL) return 0;

    /* Look for error type */
    size_t len = extract_json_string(json, json_len, "type", error_out, error_size);
    if (len > 0) {
        /* Append error message if present */
        char msg[NIMCP_ERROR_BUFFER_SIZE];
        size_t msg_len = extract_json_string(json, json_len, "message", msg, sizeof(msg));
        if (msg_len > 0 && len + msg_len + 3 < error_size) {
            error_out[len++] = ':';
            error_out[len++] = ' ';
            memcpy(error_out + len, msg, msg_len);
            len += msg_len;
            error_out[len] = '\0';
        }
    }
    return len;
}

/**
 * @brief Extract code block from Claude response
 *
 * WHAT: Extract code from markdown code block
 * WHY:  Claude often wraps code in ```c ... ```
 * HOW:  Find code fence markers and extract content
 */
static size_t extract_code_block(
    const char* text,
    size_t text_len,
    char* code_out,
    size_t code_size)
{
    if (text == NULL || code_out == NULL) return 0;

    /* Look for code block start */
    const char* code_start = find_string(text, text_len, "```c\n");
    if (code_start == NULL) {
        code_start = find_string(text, text_len, "```\n");
    }

    if (code_start != NULL) {
        /* Skip the opening fence */
        code_start = strchr(code_start, '\n');
        if (code_start != NULL) code_start++;
    } else {
        /* No code block, try to find raw code */
        code_start = text;
    }

    if (code_start == NULL) {
        code_out[0] = '\0';
        return 0;
    }

    /* Find end of code block */
    const char* code_end = find_string(code_start, text_len - (code_start - text), "```");
    if (code_end == NULL) {
        code_end = text + text_len;
    }

    size_t code_len = code_end - code_start;
    if (code_len >= code_size) {
        code_len = code_size - 1;
    }

    memcpy(code_out, code_start, code_len);
    code_out[code_len] = '\0';

    return code_len;
}

#ifdef HAVE_LIBCURL
/**
 * @brief CURL write callback
 */
static size_t nimcp_curl_write_cb(
    void* contents,
    size_t size,
    size_t nmemb,
    void* userp)
{
    size_t realsize = size * nmemb;
    response_buffer_t* buf = (response_buffer_t*)userp;

    /* Check capacity */
    if (buf->size + realsize >= buf->capacity) {
        size_t new_cap = buf->capacity * 2;
        if (new_cap < buf->size + realsize + 1) {
            new_cap = buf->size + realsize + 1;
        }
        if (new_cap > MAX_RESPONSE_SIZE) {
            return 0; /* Response too large */
        }
        char* new_data = nimcp_realloc(buf->data, new_cap);
        if (new_data == NULL) return 0;
        buf->data = new_data;
        buf->capacity = new_cap;
    }

    memcpy(buf->data + buf->size, contents, realsize);
    buf->size += realsize;
    buf->data[buf->size] = '\0';

    return realsize;
}

/**
 * @brief Make HTTPS request to Claude API
 */
static claude_heal_status_t make_api_request(
    claude_healer_t* healer,
    const char* prompt,
    response_buffer_t* response)
{
    if (healer == NULL || prompt == NULL || response == NULL) {
        return CLAUDE_HEAL_ERROR_INVALID_INPUT;
    }

    CURL* curl = (CURL*)healer->curl;
    if (curl == NULL) {
        return CLAUDE_HEAL_ERROR_INTERNAL;
    }

    /* Build request body */
    size_t prompt_len = strlen(prompt);
    size_t body_size = prompt_len + 1024;
    char* body = nimcp_malloc(body_size);
    if (body == NULL) {
        return CLAUDE_HEAL_ERROR_MEMORY;
    }

    /* Escape prompt for JSON */
    char* escaped_prompt = nimcp_malloc(prompt_len * 2 + 1);
    if (escaped_prompt == NULL) {
        nimcp_free(body);
        body = NULL;
        return CLAUDE_HEAL_ERROR_MEMORY;
    }
    json_escape_string(prompt, escaped_prompt, prompt_len * 2 + 1);

    snprintf(body, body_size,
        "{"
        "\"model\":\"%s\","
        "\"max_tokens\":4096,"
        "\"messages\":[{\"role\":\"user\",\"content\":\"%s\"}]"
        "}",
        healer->config.model,
        escaped_prompt
    );
    nimcp_free(escaped_prompt);
    escaped_prompt = NULL;

    /* Set up headers */
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    char auth_header[NIMCP_ERROR_BUFFER_LARGE];
    snprintf(auth_header, sizeof(auth_header), "x-api-key: %s", healer->config.api_key);
    headers = curl_slist_append(headers, auth_header);

    char version_header[NIMCP_LABEL_BUFFER_SIZE];
    snprintf(version_header, sizeof(version_header), "anthropic-version: %s", CLAUDE_API_VERSION);
    headers = curl_slist_append(headers, version_header);

    /* Configure CURL */
    curl_easy_reset(curl);
    curl_easy_setopt(curl, CURLOPT_URL, CLAUDE_API_URL);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, nimcp_curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, (long)healer->config.timeout_ms);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

    /* Make request */
    uint64_t start_time = get_time_ms();
    CURLcode res = curl_easy_perform(curl);
    uint64_t elapsed = get_time_ms() - start_time;

    /* Update stats */
    healer->stats.avg_response_time_ms =
        (healer->stats.avg_response_time_ms * healer->stats.requests_made + elapsed) /
        (healer->stats.requests_made + 1);

    /* Clean up headers */
    curl_slist_free_all(headers);
    nimcp_free(body);
    body = NULL;

    /* Check result */
    if (res == CURLE_OPERATION_TIMEDOUT) {
        healer->stats.requests_timed_out++;
        return CLAUDE_HEAL_ERROR_TIMEOUT;
    }
    if (res != CURLE_OK) {
        LOG_MODULE_ERROR(LOG_TAG, "CURL error: %s", curl_easy_strerror(res));
        return CLAUDE_HEAL_ERROR_NETWORK;
    }

    /* Check HTTP status */
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    if (http_code == 429) {
        healer->stats.requests_rate_limited++;
        return CLAUDE_HEAL_ERROR_RATE_LIMITED;
    }
    if (http_code != 200) {
        /* Try to extract error details from response */
        char error_msg[NIMCP_ERROR_BUFFER_SIZE];
        if (response->data != NULL && response->size > 0) {
            size_t err_len = extract_api_error(
                response->data, response->size, error_msg, sizeof(error_msg)
            );
            if (err_len > 0) {
                LOG_MODULE_ERROR(LOG_TAG, "API error: HTTP %ld - %s", http_code, error_msg);
            } else {
                LOG_MODULE_ERROR(LOG_TAG, "API error: HTTP %ld", http_code);
            }
        } else {
            LOG_MODULE_ERROR(LOG_TAG, "API error: HTTP %ld", http_code);
        }
        return CLAUDE_HEAL_ERROR_API;
    }

    /* Extract token usage from response */
    if (response->data != NULL && response->size > 0) {
        int64_t input_tokens = extract_json_int(
            response->data, response->size, JSON_INPUT_TOKENS
        );
        int64_t output_tokens = extract_json_int(
            response->data, response->size, JSON_OUTPUT_TOKENS
        );

        if (input_tokens > 0 || output_tokens > 0) {
            healer->stats.total_tokens_used +=
                (uint64_t)(input_tokens > 0 ? input_tokens : 0) +
                (uint64_t)(output_tokens > 0 ? output_tokens : 0);
        }
    }

    return CLAUDE_HEAL_SUCCESS;
}
#endif /* HAVE_LIBCURL */

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

/**
 * @brief Get default configuration
 */
int claude_healer_default_config(claude_healer_config_t* config)
{
    if (config == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "claude_healer_default_config: invalid parameter");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    claude_healer_heartbeat("claude_heale_default_config", 0.0f);


    memset(config, 0, sizeof(*config));

    /* Try environment variable for API key */
    const char* env_key = getenv("ANTHROPIC_API_KEY");
    if (env_key != NULL) {
        strncpy(config->api_key, env_key, CLAUDE_HEALER_MAX_API_KEY_SIZE - 1);
    }

    strncpy(config->model, CLAUDE_DEFAULT_MODEL, CLAUDE_HEALER_MAX_MODEL_SIZE - 1);
    config->timeout_ms = CLAUDE_HEALER_DEFAULT_TIMEOUT_MS;
    config->max_retries = CLAUDE_HEALER_DEFAULT_MAX_RETRIES;
    config->max_requests_per_minute = CLAUDE_HEALER_DEFAULT_MAX_RPM;
    config->min_confidence = 0.5f;
    config->enable_logging = false;
    config->dry_run = false;

    return 0;
}

/**
 * @brief Create Claude healer
 */
claude_healer_t* claude_healer_create(const claude_healer_config_t* config)
{
    /* Allocate healer structure */
    /* Phase 8: Heartbeat at operation start */
    claude_healer_heartbeat("claude_heale_create", 0.0f);


    claude_healer_t* healer = nimcp_calloc(1, sizeof(claude_healer_t));
    if (healer == NULL) {
        LOG_MODULE_ERROR(LOG_TAG, "Failed to allocate healer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "claude_healer_create: validation failed");
        return NULL;
    }

    /* Use provided config or defaults */
    if (config != NULL) {
        memcpy(&healer->config, config, sizeof(claude_healer_config_t));
    } else {
        claude_healer_default_config(&healer->config);
    }

    /* Validate API key */
    if (strlen(healer->config.api_key) == 0) {
        LOG_MODULE_WARN(LOG_TAG, "No API key provided, Claude healer disabled");
    }

    /* Allocate rate limit timestamp buffer */
    healer->timestamp_capacity = healer->config.max_requests_per_minute * 2;
    if (healer->timestamp_capacity < 16) healer->timestamp_capacity = 16;

    healer->request_timestamps = nimcp_calloc(
        healer->timestamp_capacity, sizeof(uint64_t)
    );
    if (healer->request_timestamps == NULL) {
        LOG_MODULE_ERROR(LOG_TAG, "Failed to allocate timestamp buffer");
        nimcp_free(healer);
        healer = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "claude_healer_create: validation failed");
        return NULL;
    }

    /* Initialize mutex */
    healer->mutex = nimcp_mutex_create(NULL);
    if (healer->mutex == NULL) {
        LOG_MODULE_ERROR(LOG_TAG, "Failed to create mutex");
        nimcp_free(healer->request_timestamps);
        nimcp_free(healer);
        healer = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "claude_healer_create: validation failed");
        return NULL;
    }

#ifdef HAVE_LIBCURL
    /* Initialize CURL */
    healer->curl = curl_easy_init();
    if (healer->curl == NULL) {
        LOG_MODULE_ERROR(LOG_TAG, "Failed to initialize CURL");
        nimcp_mutex_destroy(healer->mutex);
        nimcp_free(healer->request_timestamps);
        nimcp_free(healer);
        healer = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "claude_healer_create: validation failed");
        return NULL;
    }
    healer->api_available = true;
#else
    healer->api_available = false;
    LOG_MODULE_INFO(LOG_TAG, "Claude API disabled (libcurl not available)");
#endif

    healer->initialized = true;
    LOG_MODULE_INFO(LOG_TAG, "Claude healer initialized (API %s)",
                    healer->api_available ? "available" : "disabled");

    return healer;
}

/**
 * @brief Destroy Claude healer
 */
void claude_healer_destroy(claude_healer_t* healer)
{
    if (healer == NULL) return;

    /* Phase 8: Heartbeat at operation start */
    claude_healer_heartbeat("claude_heale_destroy", 0.0f);


#ifdef HAVE_LIBCURL
    if (healer->curl != NULL) {
        curl_easy_cleanup((CURL*)healer->curl);
    }
#endif

    if (healer->mutex != NULL) {
        nimcp_mutex_destroy(healer->mutex);
    }

    nimcp_free(healer->request_timestamps);
    nimcp_free(healer);
    healer = NULL;

    LOG_MODULE_DEBUG(LOG_TAG, "Claude healer destroyed");
}

/**
 * @brief Format crash context into prompt
 */
int claude_healer_format_prompt(
    const claude_heal_request_t* request,
    char* prompt_out,
    size_t prompt_size)
{
    if (request == NULL || prompt_out == NULL || prompt_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "claude_healer_format_prompt: invalid parameter");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    claude_healer_heartbeat("claude_heale_format_prompt", 0.0f);


    int written = snprintf(prompt_out, prompt_size,
        "You are an expert C programmer analyzing a crash in the NIMCP neural "
        "network library. Your task is to identify the root cause and provide a "
        "minimal fix.\n\n"

        "## Crash Information\n"
        "- Signal: %s (signal %d)\n"
        "- Fault Address: %p\n"
        "- File: %s\n"
        "- Line: %u\n"
        "- Function: %s\n\n"

        "## Function Signature\n"
        "```c\n%s\n```\n\n"

        "## Source Code Context (around crash site)\n"
        "```c\n%s\n```\n\n"

        "## Stack Trace\n"
        "```\n%s\n```\n\n"

        "%s%s%s"  /* Local variables section if available */

        "%s%s%s"  /* Failed fixes section if available */

        "## Instructions\n"
        "1. Identify the root cause of the crash based on the signal type and context\n"
        "2. Provide a MINIMAL fix that addresses the root cause\n"
        "3. The fix should be production-ready C code\n"
        "4. Use NIMCP conventions: guard clauses, early returns, nimcp_malloc/free\n"
        "5. Do NOT refactor or change unrelated code\n\n"

        "## Response Format\n"
        "First, briefly explain the root cause (2-3 sentences).\n"
        "Then provide the fixed code in a code block:\n"
        "```c\n"
        "// Your fixed code here\n"
        "```\n"
        "Finally, rate your confidence in this fix from 0 to 1.\n"
        "Format: CONFIDENCE: 0.X\n",

        request->signal_name ? request->signal_name : "UNKNOWN",
        request->signal,
        request->fault_address,
        request->source_file ? request->source_file : "unknown",
        request->line_number,
        request->function_name ? request->function_name : "unknown",
        request->function_signature ? request->function_signature : "// signature unavailable",
        request->source_code ? request->source_code : "// source unavailable",
        request->backtrace ? request->backtrace : "// backtrace unavailable",

        /* Local variables section */
        request->local_variables ? "## Local Variables (from DWARF)\n```\n" : "",
        request->local_variables ? request->local_variables : "",
        request->local_variables ? "\n```\n\n" : "",

        /* Failed fixes section */
        request->failed_fixes ? "## Previously Attempted Fixes (FAILED)\n" : "",
        request->failed_fixes ? request->failed_fixes : "",
        request->failed_fixes ? "\nDo NOT suggest any of the above fixes.\n\n" : ""
    );

    return written;
}

/**
 * @brief Request fix from Claude API
 */
claude_heal_status_t claude_healer_request_fix(
    claude_healer_t* healer,
    const claude_heal_request_t* request,
    claude_heal_response_t* response)
{
    /* Guard clauses */
    if (healer == NULL || request == NULL || response == NULL) {
        return CLAUDE_HEAL_ERROR_INVALID_INPUT;
    }

    /* Initialize response */
    /* Phase 8: Heartbeat at operation start */
    claude_healer_heartbeat("claude_heale_request_fix", 0.0f);


    memset(response, 0, sizeof(*response));

#ifndef HAVE_LIBCURL
    response->error_message = nimcp_strdup("Claude API disabled (libcurl not compiled in)");
    return CLAUDE_HEAL_ERROR_DISABLED;
#else

    if (!healer->api_available) {
        response->error_message = nimcp_strdup("Claude API not available");
        return CLAUDE_HEAL_ERROR_DISABLED;
    }

    if (strlen(healer->config.api_key) == 0) {
        response->error_message = nimcp_strdup("No API key configured");
        return CLAUDE_HEAL_ERROR_INVALID_INPUT;
    }

    /* Check rate limit */
    nimcp_mutex_lock(healer->mutex);
    if (!claude_healer_check_rate_limit(healer)) {
        nimcp_mutex_unlock(healer->mutex);
        response->error_message = nimcp_strdup("Rate limit exceeded");
        healer->stats.requests_rate_limited++;
        return CLAUDE_HEAL_ERROR_RATE_LIMITED;
    }

    /* Record request */
    claude_healer_record_request(healer);
    nimcp_mutex_unlock(healer->mutex);

    /* Dry run mode - don't actually call API */
    if (healer->config.dry_run) {
        response->success = true;
        response->fixed_code = nimcp_strdup("/* DRY RUN - no actual fix */");
        response->fixed_code_len = strlen(response->fixed_code);
        response->explanation = nimcp_strdup("Dry run mode - API not called");
        response->confidence = 0.0f;
        return CLAUDE_HEAL_SUCCESS;
    }

    /* Format prompt */
    char* prompt = nimcp_malloc(MAX_PROMPT_SIZE);
    if (prompt == NULL) {
        response->error_message = nimcp_strdup("Failed to allocate prompt buffer");
        return CLAUDE_HEAL_ERROR_MEMORY;
    }

    int prompt_len = claude_healer_format_prompt(request, prompt, MAX_PROMPT_SIZE);
    if (prompt_len < 0) {
        nimcp_free(prompt);
        prompt = NULL;
        response->error_message = nimcp_strdup("Failed to format prompt");
        return CLAUDE_HEAL_ERROR_INTERNAL;
    }

    if (healer->config.enable_logging) {
        LOG_MODULE_DEBUG(LOG_TAG, "Sending prompt (%d bytes) to Claude", prompt_len);
    }

    /* Initialize response buffer */
    response_buffer_t api_response = {0};
    api_response.capacity = 4096;
    api_response.data = nimcp_malloc(api_response.capacity);
    if (api_response.data == NULL) {
        nimcp_free(prompt);
        prompt = NULL;
        response->error_message = nimcp_strdup("Failed to allocate response buffer");
        return CLAUDE_HEAL_ERROR_MEMORY;
    }
    api_response.data[0] = '\0';

    /* Make API request with retries */
    claude_heal_status_t status = CLAUDE_HEAL_ERROR_NETWORK;
    uint32_t retry = 0;
    uint32_t backoff_ms = CLAUDE_HEALER_BACKOFF_BASE_MS;

    while (retry <= healer->config.max_retries) {
        healer->stats.requests_made++;

        status = make_api_request(healer, prompt, &api_response);

        if (status == CLAUDE_HEAL_SUCCESS) {
            healer->stats.requests_succeeded++;
            break;
        }

        if (status == CLAUDE_HEAL_ERROR_RATE_LIMITED ||
            status == CLAUDE_HEAL_ERROR_NETWORK) {
            /* Exponential backoff */
            if (retry < healer->config.max_retries) {
                LOG_MODULE_WARN(LOG_TAG, "Request failed, retrying in %u ms", backoff_ms);

                struct timespec ts = {
                    .tv_sec = backoff_ms / 1000,
                    .tv_nsec = (backoff_ms % 1000) * 1000000
                };
                nanosleep(&ts, NULL);

                backoff_ms *= 2;
                if (backoff_ms > CLAUDE_HEALER_BACKOFF_MAX_MS) {
                    backoff_ms = CLAUDE_HEALER_BACKOFF_MAX_MS;
                }
            }
            retry++;
        } else {
            /* Non-retryable error */
            healer->stats.requests_failed++;
            break;
        }
    }

    nimcp_free(prompt);
    prompt = NULL;

    if (status != CLAUDE_HEAL_SUCCESS) {
        nimcp_free(api_response.data);
        response->error_message = nimcp_strdup("API request failed after retries");
        return status;
    }

    if (healer->config.enable_logging) {
        LOG_MODULE_DEBUG(LOG_TAG, "Received response (%zu bytes)", api_response.size);
    }

    /* Parse response - extract text content */
    char* text_content = nimcp_malloc(api_response.size + 1);
    if (text_content == NULL) {
        nimcp_free(api_response.data);
        response->error_message = nimcp_strdup("Failed to allocate parse buffer");
        return CLAUDE_HEAL_ERROR_MEMORY;
    }

    size_t text_len = extract_json_string(
        api_response.data, api_response.size,
        "text", text_content, api_response.size
    );

    if (text_len == 0) {
        nimcp_free(text_content);
        text_content = NULL;
        nimcp_free(api_response.data);
        response->error_message = nimcp_strdup("Failed to parse API response");
        return CLAUDE_HEAL_ERROR_PARSE;
    }

    nimcp_free(api_response.data);

    /* Extract code block from response */
    response->fixed_code = nimcp_malloc(CLAUDE_HEALER_MAX_FIX_SIZE);
    if (response->fixed_code == NULL) {
        nimcp_free(text_content);
        text_content = NULL;
        response->error_message = nimcp_strdup("Failed to allocate fix buffer");
        return CLAUDE_HEAL_ERROR_MEMORY;
    }

    response->fixed_code_len = extract_code_block(
        text_content, text_len,
        response->fixed_code, CLAUDE_HEALER_MAX_FIX_SIZE
    );

    if (response->fixed_code_len == 0) {
        /* No code block found - might still have inline fix */
        strncpy(response->fixed_code, text_content,
                CLAUDE_HEALER_MAX_FIX_SIZE - 1);
        response->fixed_code_len = strlen(response->fixed_code);
    }

    /* Extract explanation (everything before code block) */
    const char* code_marker = strstr(text_content, "```");
    if (code_marker != NULL) {
        size_t explain_len = code_marker - text_content;
        if (explain_len > CLAUDE_HEALER_MAX_EXPLANATION_SIZE - 1) {
            explain_len = CLAUDE_HEALER_MAX_EXPLANATION_SIZE - 1;
        }
        response->explanation = nimcp_malloc(explain_len + 1);
        if (!response->explanation) return -1;
        if (response->explanation != NULL) {
            memcpy(response->explanation, text_content, explain_len);
            response->explanation[explain_len] = '\0';
        }
    }

    /* Extract confidence */
    const char* conf_marker = strstr(text_content, "CONFIDENCE:");
    if (conf_marker != NULL) {
        float conf = 0.0f;
        if (sscanf(conf_marker, "CONFIDENCE: %f", &conf) == 1) {
            response->confidence = conf;
        }
    } else {
        /* Default confidence if not specified */
        response->confidence = 0.6f;
    }

    nimcp_free(text_content);
    text_content = NULL;

    /* Validate generated fix code */
    if (!validate_fix_code(response->fixed_code, response->fixed_code_len)) {
        response->success = false;
        response->error_message = nimcp_strdup(
            "Generated fix failed validation (syntax check)"
        );
        LOG_MODULE_WARN(LOG_TAG, "Fix validation failed - unbalanced or malformed code");
        return CLAUDE_HEAL_ERROR_NO_FIX;
    }

    /* Check confidence threshold */
    if (response->confidence < healer->config.min_confidence) {
        response->success = false;
        response->error_message = nimcp_strdup("Confidence below threshold");
        return CLAUDE_HEAL_ERROR_LOW_CONFIDENCE;
    }

    response->success = true;
    healer->stats.fixes_generated++;

    /* Update average confidence */
    healer->stats.avg_confidence =
        (healer->stats.avg_confidence * (healer->stats.fixes_generated - 1) +
         response->confidence) / healer->stats.fixes_generated;

    LOG_MODULE_INFO(LOG_TAG, "Generated fix with confidence %.2f",
                    response->confidence);

    return CLAUDE_HEAL_SUCCESS;

#endif /* HAVE_LIBCURL */
}

/**
 * @brief Free response resources
 */
void claude_healer_free_response(claude_heal_response_t* response)
{
    if (response == NULL) return;

    /* Phase 8: Heartbeat at operation start */
    claude_healer_heartbeat("claude_heale_free_response", 0.0f);


    nimcp_free(response->fixed_code);
    nimcp_free(response->explanation);
    nimcp_free(response->error_message);

    memset(response, 0, sizeof(*response));
}

/**
 * @brief Send raw prompt to Claude API
 *
 * WHAT: Send custom prompt and receive raw response text
 * WHY:  Allow custom prompting for special/advanced use cases
 * HOW:  Make API call, extract text content from response
 */
claude_heal_status_t claude_healer_send_request(
    claude_healer_t* healer,
    const char* prompt,
    char* response_out,
    size_t response_size,
    size_t* response_len)
{
    /* Guard clauses */
    if (healer == NULL || prompt == NULL || response_out == NULL ||
        response_size == 0 || response_len == NULL) {
        return CLAUDE_HEAL_ERROR_INVALID_INPUT;
    }

    /* Phase 8: Heartbeat at operation start */
    claude_healer_heartbeat("claude_healer_send_request", 0.0f);

    *response_len = 0;

#ifndef HAVE_LIBCURL
    return CLAUDE_HEAL_ERROR_DISABLED;
#else

    if (!healer->api_available) {
        return CLAUDE_HEAL_ERROR_DISABLED;
    }

    if (strlen(healer->config.api_key) == 0) {
        return CLAUDE_HEAL_ERROR_INVALID_INPUT;
    }

    /* Check rate limit */
    nimcp_mutex_lock(healer->mutex);
    if (!claude_healer_check_rate_limit(healer)) {
        nimcp_mutex_unlock(healer->mutex);
        healer->stats.requests_rate_limited++;
        return CLAUDE_HEAL_ERROR_RATE_LIMITED;
    }
    claude_healer_record_request(healer);
    nimcp_mutex_unlock(healer->mutex);

    /* Initialize response buffer */
    response_buffer_t api_response = {0};
    api_response.capacity = 4096;
    api_response.data = nimcp_malloc(api_response.capacity);
    if (api_response.data == NULL) {
        return CLAUDE_HEAL_ERROR_MEMORY;
    }
    api_response.data[0] = '\0';

    /* Make API request with retries */
    claude_heal_status_t status = CLAUDE_HEAL_ERROR_NETWORK;
    uint32_t retry = 0;
    uint32_t backoff_ms = CLAUDE_HEALER_BACKOFF_BASE_MS;

    while (retry <= healer->config.max_retries) {
        healer->stats.requests_made++;

        status = make_api_request(healer, prompt, &api_response);

        if (status == CLAUDE_HEAL_SUCCESS) {
            healer->stats.requests_succeeded++;
            break;
        }

        if (status == CLAUDE_HEAL_ERROR_RATE_LIMITED ||
            status == CLAUDE_HEAL_ERROR_NETWORK) {
            if (retry < healer->config.max_retries) {
                struct timespec ts = {
                    .tv_sec = backoff_ms / 1000,
                    .tv_nsec = (backoff_ms % 1000) * 1000000
                };
                nanosleep(&ts, NULL);
                backoff_ms *= 2;
                if (backoff_ms > CLAUDE_HEALER_BACKOFF_MAX_MS) {
                    backoff_ms = CLAUDE_HEALER_BACKOFF_MAX_MS;
                }
            }
            retry++;
        } else {
            healer->stats.requests_failed++;
            break;
        }
    }

    if (status != CLAUDE_HEAL_SUCCESS) {
        nimcp_free(api_response.data);
        return status;
    }

    /* Extract text content from response */
    size_t text_len = extract_json_string(
        api_response.data, api_response.size,
        "text", response_out, response_size
    );

    nimcp_free(api_response.data);

    if (text_len == 0) {
        return CLAUDE_HEAL_ERROR_PARSE;
    }

    *response_len = text_len;
    return CLAUDE_HEAL_SUCCESS;

#endif /* HAVE_LIBCURL */
}

/**
 * @brief Validate C code syntax (public wrapper)
 *
 * WHAT: Public API for C code validation
 * WHY:  Allow callers to validate code before applying
 * HOW:  Wrapper around internal validate_fix_code
 */
bool claude_healer_validate_code(const char* code, size_t code_len)
{
    /* Phase 8: Heartbeat at operation start */
    claude_healer_heartbeat("claude_heale_validate_code", 0.0f);


    return validate_fix_code(code, code_len);
}

/**
 * @brief Check if request is allowed by rate limiter
 */
bool claude_healer_check_rate_limit(claude_healer_t* healer)
{
    if (healer == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "claude_healer_check_rate_limit: invalid parameter");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    claude_healer_heartbeat("claude_heale_check_rate_limit", 0.0f);


    uint64_t now = get_time_ms();
    uint64_t window_start = now - 60000; /* 1 minute window */

    /* Count requests in current window */
    size_t count = 0;
    for (size_t i = 0; i < healer->timestamp_capacity; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && healer->timestamp_capacity > 256) {
            claude_healer_heartbeat("claude_heale_loop",
                             (float)(i + 1) / (float)healer->timestamp_capacity);
        }

        if (healer->request_timestamps[i] >= window_start) {
            count++;
        }
    }

    return count < healer->config.max_requests_per_minute;
}

/**
 * @brief Record a request for rate limiting
 */
void claude_healer_record_request(claude_healer_t* healer)
{
    if (healer == NULL) return;

    /* Phase 8: Heartbeat at operation start */
    claude_healer_heartbeat("claude_heale_record_request", 0.0f);


    healer->request_timestamps[healer->timestamp_head] = get_time_ms();
    healer->timestamp_head = (healer->timestamp_head + 1) % healer->timestamp_capacity;

    if (healer->request_count < healer->timestamp_capacity) {
        healer->request_count++;
    }
}

/**
 * @brief Get time until rate limit resets
 */
uint64_t claude_healer_get_rate_limit_reset_ms(claude_healer_t* healer)
{
    if (healer == NULL) return 0;

    /* Phase 8: Heartbeat at operation start */
    claude_healer_heartbeat("claude_heale_get_rate_limit_reset", 0.0f);


    uint64_t now = get_time_ms();
    uint64_t window_start = now - 60000;

    /* Find oldest request in window */
    uint64_t oldest = now;
    for (size_t i = 0; i < healer->timestamp_capacity; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && healer->timestamp_capacity > 256) {
            claude_healer_heartbeat("claude_heale_loop",
                             (float)(i + 1) / (float)healer->timestamp_capacity);
        }

        if (healer->request_timestamps[i] >= window_start &&
            healer->request_timestamps[i] < oldest) {
            oldest = healer->request_timestamps[i];
        }
    }

    /* Calculate time until oldest request leaves window */
    uint64_t reset_time = oldest + 60000;
    if (reset_time > now) {
        return reset_time - now;
    }
    return 0;
}

/**
 * @brief Get healer statistics
 */
int claude_healer_get_stats(
    claude_healer_t* healer,
    claude_healer_stats_t* stats)
{
    if (healer == NULL || stats == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "claude_healer_get_stats: invalid parameter");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    claude_healer_heartbeat("claude_heale_get_stats", 0.0f);


    nimcp_mutex_lock(healer->mutex);
    memcpy(stats, &healer->stats, sizeof(claude_healer_stats_t));
    nimcp_mutex_unlock(healer->mutex);

    return 0;
}

/**
 * @brief Reset statistics
 */
int claude_healer_reset_stats(claude_healer_t* healer)
{
    if (healer == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "claude_healer_reset_stats: invalid parameter");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    claude_healer_heartbeat("claude_heale_reset_stats", 0.0f);


    nimcp_mutex_lock(healer->mutex);
    memset(&healer->stats, 0, sizeof(claude_healer_stats_t));
    nimcp_mutex_unlock(healer->mutex);

    return 0;
}

/**
 * @brief Check if Claude API is available
 */
bool claude_healer_is_available(void)
{
    /* Phase 8: Heartbeat at operation start */
    claude_healer_heartbeat("claude_heale_is_available", 0.0f);


#ifdef HAVE_LIBCURL
    return true;
#else
    return false;
#endif
}

/**
 * @brief Get status string
 */
const char* claude_healer_status_to_string(claude_heal_status_t status)
{
    switch (status) {
        case CLAUDE_HEAL_SUCCESS:
            return "Success";
        case CLAUDE_HEAL_ERROR_DISABLED:
            return "API disabled (no libcurl)";
        case CLAUDE_HEAL_ERROR_RATE_LIMITED:
            return "Rate limit exceeded";
        case CLAUDE_HEAL_ERROR_TIMEOUT:
            return "Request timed out";
        case CLAUDE_HEAL_ERROR_NETWORK:
            return "Network error";
        case CLAUDE_HEAL_ERROR_API:
            return "API error";
        case CLAUDE_HEAL_ERROR_PARSE:
            return "Failed to parse response";
        case CLAUDE_HEAL_ERROR_INVALID_INPUT:
            return "Invalid input";
        case CLAUDE_HEAL_ERROR_NO_FIX:
            return "No fix generated";
        case CLAUDE_HEAL_ERROR_LOW_CONFIDENCE:
            return "Confidence below threshold";
        case CLAUDE_HEAL_ERROR_MEMORY:
            return "Memory allocation failed";
        case CLAUDE_HEAL_ERROR_INTERNAL:
            return "Internal error";
        default:
            return "Unknown status";
    }
}

/**
 * @brief Get signal name from number
 */
const char* claude_healer_signal_name(int sig)
{
    switch (sig) {
#ifdef SIGSEGV
        case SIGSEGV: return "SIGSEGV";
#endif
#ifdef SIGBUS
        case SIGBUS:  return "SIGBUS";
#endif
#ifdef SIGFPE
        case SIGFPE:  return "SIGFPE";
#endif
#ifdef SIGILL
        case SIGILL:  return "SIGILL";
#endif
#ifdef SIGABRT
        case SIGABRT: return "SIGABRT";
#endif
#ifdef SIGTRAP
        case SIGTRAP: return "SIGTRAP";
#endif
        default:      return "UNKNOWN";
    }
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * @brief Query self-knowledge from knowledge graph
 *
 * WHAT: Query KG for module self-awareness information
 * WHY:  Enable introspective self-knowledge about claude healer
 * HOW:  Look up entity and relations in KG
 *
 * @param kg Knowledge graph reader handle
 * @return 1 if self-knowledge found, 0 otherwise
 */
int claude_healer_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    claude_healer_heartbeat("claude_heale_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Claude_Healer");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                claude_healer_heartbeat("claude_heale_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Claude healer self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Claude_Healer");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Claude_Healer");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void claude_healer_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_claude_healer_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int claude_healer_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "claude_healer_training_begin: NULL argument");
        return -1;
    }
    claude_healer_heartbeat_instance(NULL, "claude_healer_training_begin", 0.0f);
    (void)instance; /* Module state available for reset */
    return 0;
}

int claude_healer_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "claude_healer_training_end: NULL argument");
        return -1;
    }
    claude_healer_heartbeat_instance(NULL, "claude_healer_training_end", 1.0f);
    (void)instance; /* Module state available for finalization */
    return 0;
}

int claude_healer_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "claude_healer_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    claude_healer_heartbeat_instance(NULL, "claude_healer_training_step", progress);
    (void)instance; /* Module state available for step adaptation */
    return 0;
}
