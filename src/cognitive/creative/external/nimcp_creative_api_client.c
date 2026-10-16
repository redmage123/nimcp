//=============================================================================
// nimcp_creative_api_client.c - Cloud API Integration Implementation
//=============================================================================
/**
 * @file nimcp_creative_api_client.c
 * @brief Implements client for cloud-based creative AI APIs
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-01-30
 */

#include "cognitive/creative/external/nimcp_creative_api_client.h"
#include "utils/memory/nimcp_memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "utils/fault_tolerance/nimcp_recovery.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "constants/nimcp_buffer_constants.h"
#include "constants/nimcp_timing_constants.h"
#include <math.h>

//=============================================================================
// Circuit Breaker for External API Protection
//=============================================================================

/**
 * @brief Global circuit breaker for external API calls
 *
 * WHAT: Circuit breaker to prevent cascading failures from external API issues
 * WHY:  Cloud APIs can be unreliable; stop hammering them when they're down
 * HOW:  Track consecutive failures, open circuit after threshold, test periodically
 */
static circuit_breaker_t* g_api_circuit_breaker = NULL;

/* Circuit breaker configuration constants */
#define API_CB_FAILURE_THRESHOLD    5      /**< Failures before opening circuit */
#define API_CB_TIMEOUT_MS           NIMCP_LONG_TIMEOUT_MS  /**< 30 seconds before testing recovery */

/**
 * @brief Initialize the API circuit breaker
 *
 * @return 0 on success, -1 on failure
 */
static int init_api_circuit_breaker(void) {
    if (g_api_circuit_breaker != NULL) {
        return 0;  /* Already initialized */
    }

    g_api_circuit_breaker = circuit_breaker_create(
        API_CB_FAILURE_THRESHOLD,
        API_CB_TIMEOUT_MS
    );

    return g_api_circuit_breaker != NULL ? 0 : -1;
}

/**
 * @brief Cleanup the API circuit breaker
 */
static void cleanup_api_circuit_breaker(void) {
    if (g_api_circuit_breaker != NULL) {
        circuit_breaker_destroy(g_api_circuit_breaker);
        g_api_circuit_breaker = NULL;
    }
}

BRIDGE_BOILERPLATE_MESH_ONLY(creative_api_client, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Configuration Defaults
//=============================================================================

void creative_api_client_config_defaults(creative_api_client_config_t* config)
{
    if (!config) return;

    memset(config, 0, sizeof(creative_api_client_config_t));

    config->provider = API_PROVIDER_STABILITY;

    config->timeout_ms = 60000;
    config->max_retries = 3;
    config->retry_delay_ms = 1000;

    config->requests_per_minute = 0;  /* Unlimited */

    config->verify_ssl = true;
}

//=============================================================================
// Lifecycle
//=============================================================================

creative_api_client_t* creative_api_client_create(
    const creative_api_client_config_t* config)
{
    /* Initialize circuit breaker on first client creation */
    init_api_circuit_breaker();

    creative_api_client_t* client = nimcp_calloc(1, sizeof(creative_api_client_t));
    if (!client) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "creative_api_client_create: client is NULL");
        return NULL;
    }

    if (config) {
        client->config = *config;
    } else {
        creative_api_client_config_defaults(&client->config);
    }

    /* Set default base URLs if not specified */
    if (strlen(client->config.base_url) == 0) {
        switch (client->config.provider) {
            case API_PROVIDER_STABILITY:
                strncpy(client->config.base_url,
                        "https://api.stability.ai/v1",
                        sizeof(client->config.base_url) - 1);
                break;

            case API_PROVIDER_OPENAI:
                strncpy(client->config.base_url,
                        "https://api.openai.com/v1",
                        sizeof(client->config.base_url) - 1);
                break;

            case API_PROVIDER_REPLICATE:
                strncpy(client->config.base_url,
                        "https://api.replicate.com/v1",
                        sizeof(client->config.base_url) - 1);
                break;

            case API_PROVIDER_ANTHROPIC:
                strncpy(client->config.base_url,
                        "https://api.anthropic.com/v1",
                        sizeof(client->config.base_url) - 1);
                break;

            case API_PROVIDER_HUGGINGFACE:
                strncpy(client->config.base_url,
                        "https://api-inference.huggingface.co/models",
                        sizeof(client->config.base_url) - 1);
                break;

            default:
                break;
        }
    }

    client->http_client = NULL;  /* Would be curl/http client handle */

    client->last_request_time = 0;
    client->requests_this_minute = 0;
    client->minute_start_time = 0;

    client->total_requests = 0;
    client->successful_requests = 0;
    client->failed_requests = 0;
    client->avg_response_time_ms = 0.0f;

    return client;
}

void creative_api_client_destroy(creative_api_client_t* client)
{
    if (!client) return;

    /* Cleanup HTTP client */
    if (client->http_client) {
        /* Would cleanup curl/http client */
    }

    nimcp_free(client);
    client = NULL;
}

int creative_api_client_test(creative_api_client_t* client)
{
    if (!client) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "creative_api_client_test: client is NULL");
        return -1;
    }

    /* Verify credentials are set */
    if (strlen(client->config.credentials.api_key) == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "creative_api_client_test: validation failed");
        return -1;
    }

    /* Would make a test API call here */
    return 0;
}

//=============================================================================
// Internal: Rate Limiting
//=============================================================================

static bool check_rate_limit(creative_api_client_t* client)
{
    if (client->config.requests_per_minute == 0) {
        return true;  /* No limit */
    }

    uint64_t now = (uint64_t)time(NULL);

    /* Reset counter if minute has passed */
    if (now - client->minute_start_time >= 60) {
        client->minute_start_time = now;
        client->requests_this_minute = 0;
    }

    return client->requests_this_minute < client->config.requests_per_minute;
}

static void record_request(creative_api_client_t* client)
{
    client->last_request_time = (uint64_t)time(NULL);
    client->requests_this_minute++;
    client->total_requests++;
}

//=============================================================================
// Internal: HTTP Helpers (Placeholder)
//=============================================================================

/**
 * @brief Placeholder for HTTP POST request
 */
static int http_post(const char* url, const char* api_key,
                     const char* body, size_t body_len,
                     char** response, size_t* response_len)
{
    (void)url;
    (void)api_key;
    (void)body;
    (void)body_len;

    /* Placeholder: would use libcurl or similar */
    *response = nimcp_calloc(256, sizeof(char));
    if (*response) {
        strncpy(*response, "{\"status\":\"success\"}", 255);
        *response_len = strlen(*response);
    }

    return 0;
}

//=============================================================================
// Image Generation API
//=============================================================================

int api_generate_image(creative_api_client_t* client,
                        const api_image_request_t* request,
                        api_image_response_t* response)
{
    if (!client || !request || !response) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "api_generate_image: required parameter is NULL (client, request, response)");
        return -1;
    }

    memset(response, 0, sizeof(api_image_response_t));

    /* Circuit breaker check - prevent calls when API is known to be failing */
    if (g_api_circuit_breaker && !circuit_breaker_allow_operation(g_api_circuit_breaker)) {
        response->status = API_STATUS_SERVER_ERROR;
        strncpy(response->error_message, "Circuit breaker open - API unavailable",
                sizeof(response->error_message) - 1);
        response->retry_after_ms = API_CB_TIMEOUT_MS;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "api_generate_image: circuit_breaker_allow_operation is NULL");
        return -1;
    }

    /* Check rate limit */
    if (!check_rate_limit(client)) {
        response->status = API_STATUS_RATE_LIMITED;
        response->retry_after_ms = 60000 - (uint32_t)((uint64_t)time(NULL) -
                                                       client->minute_start_time) * 1000;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "api_generate_image: check_rate_limit is NULL");
        return -1;
    }

    /* Check credentials */
    if (strlen(client->config.credentials.api_key) == 0) {
        response->status = API_STATUS_AUTH_ERROR;
        strncpy(response->error_message, "API key not set",
                sizeof(response->error_message) - 1);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "api_generate_image: validation failed");
        return -1;
    }

    clock_t start = clock();

    /* Build request based on provider */
    char url[NIMCP_ERROR_BUFFER_LARGE];
    char body[NIMCP_PATH_BUFFER_SIZE];

    switch (client->config.provider) {
        case API_PROVIDER_STABILITY:
            snprintf(url, sizeof(url), "%s/generation/stable-diffusion-xl-1024-v1-0/text-to-image",
                     client->config.base_url);
            snprintf(body, sizeof(body),
                     "{\"text_prompts\":[{\"text\":\"%s\"}],\"width\":%u,\"height\":%u,\"steps\":%u}",
                     request->prompt,
                     request->width > 0 ? request->width : 1024,
                     request->height > 0 ? request->height : 1024,
                     request->steps > 0 ? request->steps : 30);
            break;

        case API_PROVIDER_OPENAI:
            snprintf(url, sizeof(url), "%s/images/generations",
                     client->config.base_url);
            snprintf(body, sizeof(body),
                     "{\"model\":\"dall-e-3\",\"prompt\":\"%s\",\"size\":\"%ux%u\",\"n\":1}",
                     request->prompt,
                     request->width > 0 ? request->width : 1024,
                     request->height > 0 ? request->height : 1024);
            break;

        case API_PROVIDER_REPLICATE:
            snprintf(url, sizeof(url), "%s/predictions",
                     client->config.base_url);
            snprintf(body, sizeof(body),
                     "{\"version\":\"sdxl\",\"input\":{\"prompt\":\"%s\"}}",
                     request->prompt);
            break;

        default:
            response->status = API_STATUS_INVALID_REQUEST;
            strncpy(response->error_message, "Unsupported provider",
                    sizeof(response->error_message) - 1);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "api_generate_image: operation failed");
            return -1;
    }

    /* Make request */
    char* http_response = NULL;
    size_t http_response_len = 0;

    int rc = http_post(url, client->config.credentials.api_key,
                       body, strlen(body),
                       &http_response, &http_response_len);

    record_request(client);

    if (rc != 0 || !http_response) {
        /* Record failure in circuit breaker */
        if (g_api_circuit_breaker) {
            circuit_breaker_record_failure(g_api_circuit_breaker);
        }
        response->status = API_STATUS_NETWORK_ERROR;
        strncpy(response->error_message, "Network request failed",
                sizeof(response->error_message) - 1);
        client->failed_requests++;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "api_generate_image: validation failed");
        return -1;
    }

    /* Record success in circuit breaker */
    if (g_api_circuit_breaker) {
        circuit_breaker_record_success(g_api_circuit_breaker);
    }

    /* Parse response (placeholder - would parse JSON) */
    response->status = API_STATUS_SUCCESS;
    response->num_images = 1;
    response->images = nimcp_calloc(1, sizeof(visual_image_t));

    if (response->images) {
        /* Generate placeholder image */
        uint32_t w = request->width > 0 ? request->width : 1024;
        uint32_t h = request->height > 0 ? request->height : 1024;

        response->images[0].width = w;
        response->images[0].height = h;
        response->images[0].channels = 3;
        response->images[0].pixels = nimcp_calloc(w * h * 3, sizeof(uint8_t));

        if (response->images[0].pixels) {
            /* Fill with pattern */
            uint64_t state = request->seed ? request->seed : (uint64_t)time(NULL);
            for (uint32_t y = 0; y < h; y++) {
                for (uint32_t x = 0; x < w; x++) {
                    size_t idx = (y * w + x) * 3;
                    state = state * 6364136223846793005ULL + 1442695040888963407ULL;
                    response->images[0].pixels[idx + 0] = (uint8_t)(state >> 48);
                    response->images[0].pixels[idx + 1] = (uint8_t)(state >> 40);
                    response->images[0].pixels[idx + 2] = (uint8_t)(state >> 32);
                }
            }
        }
    }

    response->seed_used = request->seed ? request->seed : (uint64_t)time(NULL);
    response->generation_time_ms = (float)(clock() - start) * 1000.0f / CLOCKS_PER_SEC;

    nimcp_free(http_response);
    http_response = NULL;

    /* Update statistics */
    client->successful_requests++;
    float n = (float)client->successful_requests;
    client->avg_response_time_ms = client->avg_response_time_ms * ((n-1)/(fabsf(n) > 1e-7f ? n : 1e-7f)) +
                                    response->generation_time_ms / (fabsf(n) > 1e-7f ? n : 1e-7f);

    return 0;
}

int api_generate_image_async(creative_api_client_t* client,
                              const api_image_request_t* request,
                              api_image_callback_t callback,
                              void* user_data)
{
    if (!client || !request || !callback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "api_generate_image_async: required parameter is NULL (client, request, callback)");
        return -1;
    }

    /* Placeholder: run synchronously and call callback */
    api_image_response_t response;
    int rc = api_generate_image(client, request, &response);
    callback(&response, user_data);

    return rc;
}

int api_upscale_image(creative_api_client_t* client,
                       const visual_image_t* image,
                       uint32_t scale,
                       api_image_response_t* response)
{
    if (!client || !image || !response) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "api_upscale_image: required parameter is NULL (client, image, response)");
        return -1;
    }

    memset(response, 0, sizeof(api_image_response_t));

    /* Placeholder upscaling */
    uint32_t new_width = image->width * scale;
    uint32_t new_height = image->height * scale;

    response->status = API_STATUS_SUCCESS;
    response->num_images = 1;
    response->images = nimcp_calloc(1, sizeof(visual_image_t));

    if (response->images) {
        response->images[0].width = new_width;
        response->images[0].height = new_height;
        response->images[0].channels = image->channels;
        response->images[0].pixels = nimcp_calloc((size_t)new_width * (size_t)new_height *
                                                   (size_t)image->channels, sizeof(uint8_t));

        if (response->images[0].pixels) {
            /* Simple nearest-neighbor upscale */
            for (uint32_t y = 0; y < new_height; y++) {
                for (uint32_t x = 0; x < new_width; x++) {
                    uint32_t src_x = x / (scale > 0 ? scale : 1);
                    uint32_t src_y = y / (scale > 0 ? scale : 1);

                    src_x = src_x < image->width ? src_x : image->width - 1;
                    src_y = src_y < image->height ? src_y : image->height - 1;

                    size_t src_idx = (src_y * image->width + src_x) * image->channels;
                    size_t dst_idx = (y * new_width + x) * image->channels;

                    for (uint32_t c = 0; c < image->channels; c++) {
                        response->images[0].pixels[dst_idx + c] =
                            image->pixels[src_idx + c];
                    }
                }
            }
        }
    }

    return 0;
}

//=============================================================================
// Text Generation API
//=============================================================================

int api_generate_text(creative_api_client_t* client,
                       const api_text_request_t* request,
                       api_text_response_t* response)
{
    if (!client || !request || !response) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "api_generate_text: required parameter is NULL (client, request, response)");
        return -1;
    }

    memset(response, 0, sizeof(api_text_response_t));

    /* Circuit breaker check - prevent calls when API is known to be failing */
    if (g_api_circuit_breaker && !circuit_breaker_allow_operation(g_api_circuit_breaker)) {
        response->status = API_STATUS_SERVER_ERROR;
        strncpy(response->error_message, "Circuit breaker open - API unavailable",
                sizeof(response->error_message) - 1);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "api_generate_text: circuit_breaker_allow_operation is NULL");
        return -1;
    }

    /* Check rate limit */
    if (!check_rate_limit(client)) {
        response->status = API_STATUS_RATE_LIMITED;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "api_generate_text: check_rate_limit is NULL");
        return -1;
    }

    /* Check credentials */
    if (strlen(client->config.credentials.api_key) == 0) {
        response->status = API_STATUS_AUTH_ERROR;
        strncpy(response->error_message, "API key not set",
                sizeof(response->error_message) - 1);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "api_generate_text: validation failed");
        return -1;
    }

    clock_t start = clock();

    /* Build request based on provider */
    char url[NIMCP_ERROR_BUFFER_LARGE];
    char body[8192];

    switch (client->config.provider) {
        case API_PROVIDER_OPENAI:
            snprintf(url, sizeof(url), "%s/chat/completions",
                     client->config.base_url);
            snprintf(body, sizeof(body),
                     "{\"model\":\"%s\",\"messages\":[{\"role\":\"user\",\"content\":\"%s\"}],\"max_tokens\":%u,\"temperature\":%.2f}",
                     request->model ? request->model : "gpt-4",
                     request->prompt,
                     request->max_tokens > 0 ? request->max_tokens : 1000,
                     request->temperature > 0 ? request->temperature : 0.7f);
            break;

        case API_PROVIDER_ANTHROPIC:
            snprintf(url, sizeof(url), "%s/messages",
                     client->config.base_url);
            snprintf(body, sizeof(body),
                     "{\"model\":\"%s\",\"max_tokens\":%u,\"messages\":[{\"role\":\"user\",\"content\":\"%s\"}]}",
                     request->model ? request->model : "claude-3-opus-20240229",
                     request->max_tokens > 0 ? request->max_tokens : 1000,
                     request->prompt);
            break;

        default:
            response->status = API_STATUS_INVALID_REQUEST;
            strncpy(response->error_message, "Provider doesn't support text generation",
                    sizeof(response->error_message) - 1);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "api_generate_text: operation failed");
            return -1;
    }

    /* Make request */
    char* http_response = NULL;
    size_t http_response_len = 0;

    int rc = http_post(url, client->config.credentials.api_key,
                       body, strlen(body),
                       &http_response, &http_response_len);

    record_request(client);

    if (rc != 0 || !http_response) {
        /* Record failure in circuit breaker */
        if (g_api_circuit_breaker) {
            circuit_breaker_record_failure(g_api_circuit_breaker);
        }
        response->status = API_STATUS_NETWORK_ERROR;
        client->failed_requests++;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "api_generate_text: validation failed");
        return -1;
    }

    /* Record success in circuit breaker */
    if (g_api_circuit_breaker) {
        circuit_breaker_record_success(g_api_circuit_breaker);
    }

    /* Parse response (placeholder) */
    response->status = API_STATUS_SUCCESS;
    response->text = nimcp_calloc(1024, sizeof(char));
    if (response->text) {
        strncpy(response->text,
                "This is a placeholder response from the API client. "
                "In production, this would contain the actual generated text.",
                1023);
        response->text_len = strlen(response->text);
    }

    response->tokens_used = 100;  /* Placeholder */
    response->generation_time_ms = (float)(clock() - start) * 1000.0f / CLOCKS_PER_SEC;

    nimcp_free(http_response);
    http_response = NULL;
    client->successful_requests++;

    return 0;
}

int api_generate_text_stream(creative_api_client_t* client,
                              const api_text_request_t* request,
                              api_token_callback_t token_callback,
                              api_complete_callback_t complete_callback,
                              void* user_data)
{
    if (!client || !request || !complete_callback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "api_generate_text_stream: required parameter is NULL (client, request, complete_callback)");
        return -1;
    }

    /* Placeholder: generate text and simulate streaming */
    api_text_response_t response;
    int rc = api_generate_text(client, request, &response);

    /* Simulate token streaming */
    if (token_callback && response.text) {
        /* strdup the string before tokenizing — strtok modifies in-place and
           would corrupt the response text before the callback receives it */
        char* text_copy = strdup(response.text);
        if (text_copy) {
            char* word = strtok(text_copy, " ");
            while (word) {
                token_callback(word, user_data);
                token_callback(" ", user_data);
                word = strtok(NULL, " ");
            }
            free(text_copy);
        }
    }

    complete_callback(&response, user_data);

    return rc;
}

//=============================================================================
// Provider-Specific API
//=============================================================================

uint32_t api_list_models(creative_api_client_t* client,
                          char** models,
                          uint32_t max_models)
{
    if (!client || !models) return 0;

    /* Return provider-specific model list */
    uint32_t count = 0;

    switch (client->config.provider) {
        case API_PROVIDER_STABILITY:
            if (count < max_models) models[count++] = "stable-diffusion-xl-1024-v1-0";
            if (count < max_models) models[count++] = "stable-diffusion-v1-6";
            if (count < max_models) models[count++] = "stable-diffusion-512-v2-1";
            break;

        case API_PROVIDER_OPENAI:
            if (count < max_models) models[count++] = "dall-e-3";
            if (count < max_models) models[count++] = "dall-e-2";
            if (count < max_models) models[count++] = "gpt-4";
            if (count < max_models) models[count++] = "gpt-4-turbo";
            break;

        case API_PROVIDER_ANTHROPIC:
            if (count < max_models) models[count++] = "claude-3-opus-20240229";
            if (count < max_models) models[count++] = "claude-3-sonnet-20240229";
            if (count < max_models) models[count++] = "claude-3-haiku-20240307";
            break;

        default:
            break;
    }

    return count;
}

uint32_t api_get_capabilities(const creative_api_client_t* client)
{
    if (!client) return 0;

    switch (client->config.provider) {
        case API_PROVIDER_STABILITY:
            return API_CAP_TEXT_TO_IMAGE | API_CAP_IMG_TO_IMG |
                   API_CAP_INPAINTING | API_CAP_UPSCALE;

        case API_PROVIDER_OPENAI:
            return API_CAP_TEXT_TO_IMAGE | API_CAP_TEXT_GEN | API_CAP_EMBEDDING;

        case API_PROVIDER_REPLICATE:
            return API_CAP_TEXT_TO_IMAGE | API_CAP_IMG_TO_IMG |
                   API_CAP_VIDEO_GEN | API_CAP_MUSIC_GEN;

        case API_PROVIDER_ANTHROPIC:
            return API_CAP_TEXT_GEN;

        case API_PROVIDER_HUGGINGFACE:
            return API_CAP_TEXT_TO_IMAGE | API_CAP_TEXT_GEN | API_CAP_EMBEDDING;

        default:
            return 0;
    }
}

int api_get_balance(creative_api_client_t* client,
                     float* balance,
                     char* currency)
{
    if (!client || !balance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "api_get_balance: required parameter is NULL (client, balance)");
        return -1;
    }

    /* Placeholder - would query API */
    *balance = 100.0f;
    if (currency) {
        strncpy(currency, "USD", 4);
    }

    return 0;
}

//=============================================================================
// Utility API
//=============================================================================

bool api_is_rate_limited(const creative_api_client_t* client)
{
    if (!client || client->config.requests_per_minute == 0) {
        return false;
    }

    uint64_t now = (uint64_t)time(NULL);
    if (now - client->minute_start_time >= 60) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "api_is_rate_limited: capacity exceeded");
        return false;  /* Minute has passed */
    }

    return client->requests_this_minute >= client->config.requests_per_minute;
}

uint32_t api_rate_limit_reset_ms(const creative_api_client_t* client)
{
    if (!client || !api_is_rate_limited(client)) {
        return 0;
    }

    uint64_t now = (uint64_t)time(NULL);
    uint64_t elapsed = now - client->minute_start_time;

    if (elapsed >= 60) return 0;

    return (uint32_t)(60 - elapsed) * 1000;
}

const char* api_status_string(api_status_t status)
{
    switch (status) {
        case API_STATUS_SUCCESS:         return "Success";
        case API_STATUS_PENDING:         return "Pending";
        case API_STATUS_RATE_LIMITED:    return "Rate limited";
        case API_STATUS_AUTH_ERROR:      return "Authentication error";
        case API_STATUS_INVALID_REQUEST: return "Invalid request";
        case API_STATUS_SERVER_ERROR:    return "Server error";
        case API_STATUS_TIMEOUT:         return "Timeout";
        case API_STATUS_NETWORK_ERROR:   return "Network error";
        case API_STATUS_CONTENT_FILTERED: return "Content filtered";
        case API_STATUS_QUOTA_EXCEEDED:  return "Quota exceeded";
        default:                         return "Unknown";
    }
}

void api_image_response_free(api_image_response_t* response)
{
    if (!response) return;

    if (response->images) {
        for (uint32_t i = 0; i < response->num_images; i++) {
            if (response->images[i].pixels) {
                nimcp_free(response->images[i].pixels);
            }
        }
        nimcp_free(response->images);
        response->images = NULL;
    }

    response->num_images = 0;
}

void api_text_response_free(api_text_response_t* response)
{
    if (!response) return;

    if (response->text) {
        nimcp_free(response->text);
        response->text = NULL;
    }

    response->text_len = 0;
}
