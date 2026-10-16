//=============================================================================
// nimcp_creative_api_client.h - Cloud API Integration for Creative Models
//=============================================================================
/**
 * @file nimcp_creative_api_client.h
 * @brief Client for cloud-based creative AI APIs
 *
 * WHAT: Interface to cloud AI services for generation
 * WHY:  Fallback when local models unavailable, access to latest models
 * HOW:  HTTP client with API-specific request formatting
 *
 * SUPPORTED APIS:
 * - Stability AI: Stable Diffusion API
 * - OpenAI: DALL-E, GPT for text
 * - Replicate: Various models
 * - Anthropic: Claude for text generation
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-01-30
 */

#ifndef NIMCP_CREATIVE_API_CLIENT_H
#define NIMCP_CREATIVE_API_CLIENT_H

#include "cognitive/creative/nimcp_creative.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// API Provider Types
//=============================================================================

/**
 * @brief API provider types
 */
typedef enum {
    API_PROVIDER_STABILITY = 0,    /**< Stability AI */
    API_PROVIDER_OPENAI,           /**< OpenAI */
    API_PROVIDER_REPLICATE,        /**< Replicate */
    API_PROVIDER_ANTHROPIC,        /**< Anthropic */
    API_PROVIDER_HUGGINGFACE,      /**< Hugging Face Inference */
    API_PROVIDER_CUSTOM            /**< Custom endpoint */
} api_provider_t;

/**
 * @brief API capability flags
 */
typedef enum {
    API_CAP_TEXT_TO_IMAGE = 0x01,  /**< Text-to-image generation */
    API_CAP_IMG_TO_IMG    = 0x02,  /**< Image-to-image */
    API_CAP_INPAINTING    = 0x04,  /**< Inpainting */
    API_CAP_TEXT_GEN      = 0x08,  /**< Text generation */
    API_CAP_MUSIC_GEN     = 0x10,  /**< Music generation */
    API_CAP_VIDEO_GEN     = 0x20,  /**< Video generation */
    API_CAP_EMBEDDING     = 0x40,  /**< Embedding computation */
    API_CAP_UPSCALE       = 0x80   /**< Image upscaling */
} api_capability_t;

//=============================================================================
// Request/Response Types
//=============================================================================

/**
 * @brief API request status
 */
typedef enum {
    API_STATUS_SUCCESS = 0,        /**< Success */
    API_STATUS_PENDING,            /**< Request pending */
    API_STATUS_RATE_LIMITED,       /**< Rate limited, retry later */
    API_STATUS_AUTH_ERROR,         /**< Authentication error */
    API_STATUS_INVALID_REQUEST,    /**< Invalid request */
    API_STATUS_SERVER_ERROR,       /**< Server error */
    API_STATUS_TIMEOUT,            /**< Request timeout */
    API_STATUS_NETWORK_ERROR,      /**< Network error */
    API_STATUS_CONTENT_FILTERED,   /**< Content filtered by safety */
    API_STATUS_QUOTA_EXCEEDED      /**< Quota exceeded */
} api_status_t;

/**
 * @brief Image generation API request
 */
typedef struct {
    const char* prompt;            /**< Generation prompt */
    const char* negative_prompt;   /**< Negative prompt (optional) */
    uint32_t width;                /**< Output width */
    uint32_t height;               /**< Output height */
    uint32_t steps;                /**< Inference steps */
    float guidance_scale;          /**< CFG scale */
    uint64_t seed;                 /**< Random seed (0 for random) */
    const char* model;             /**< Model name (provider-specific) */
    const char* style_preset;      /**< Style preset (optional) */
    visual_image_t* init_image;    /**< Init image for img2img (optional) */
    visual_image_t* mask;          /**< Mask for inpainting (optional) */
    float strength;                /**< Denoising strength for img2img */
} api_image_request_t;

/**
 * @brief Image generation API response
 */
typedef struct {
    api_status_t status;           /**< Response status */
    visual_image_t* images;        /**< Generated images */
    uint32_t num_images;           /**< Number of images */
    uint64_t seed_used;            /**< Seed used */
    float generation_time_ms;      /**< Server generation time */
    char error_message[256];       /**< Error message if failed */
    uint32_t retry_after_ms;       /**< Retry after (if rate limited) */
} api_image_response_t;

/**
 * @brief Text generation API request
 */
typedef struct {
    const char* prompt;            /**< Generation prompt */
    const char* system_prompt;     /**< System prompt (optional) */
    uint32_t max_tokens;           /**< Max tokens to generate */
    float temperature;             /**< Sampling temperature */
    float top_p;                   /**< Nucleus sampling */
    const char* model;             /**< Model name */
    bool stream;                   /**< Stream response */
} api_text_request_t;

/**
 * @brief Text generation API response
 */
typedef struct {
    api_status_t status;           /**< Response status */
    char* text;                    /**< Generated text (heap allocated) */
    size_t text_len;               /**< Text length */
    uint32_t tokens_used;          /**< Tokens used */
    float generation_time_ms;      /**< Server generation time */
    char error_message[256];       /**< Error message if failed */
} api_text_response_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief API credentials
 */
typedef struct {
    char api_key[256];             /**< API key */
    char organization[128];        /**< Organization ID (optional) */
} api_credentials_t;

/**
 * @brief API client configuration
 */
typedef struct {
    api_provider_t provider;       /**< API provider */
    api_credentials_t credentials; /**< Credentials */
    char base_url[256];            /**< Base URL (or empty for default) */

    /* Request settings */
    uint32_t timeout_ms;           /**< Request timeout */
    uint32_t max_retries;          /**< Max retry attempts */
    uint32_t retry_delay_ms;       /**< Delay between retries */

    /* Rate limiting */
    uint32_t requests_per_minute;  /**< Self-imposed rate limit (0=unlimited) */

    /* Proxy settings */
    char proxy_url[256];           /**< Proxy URL (optional) */

    /* SSL settings */
    bool verify_ssl;               /**< Verify SSL certificates */
    char ca_cert_path[256];        /**< CA certificate path (optional) */
} creative_api_client_config_t;

/**
 * @brief Initialize config with defaults
 */
void creative_api_client_config_defaults(creative_api_client_config_t* config);

//=============================================================================
// Client Structure
//=============================================================================

/**
 * @brief Creative API client
 */
struct creative_api_client {
    creative_api_client_config_t config;

    /* HTTP client (opaque) */
    void* http_client;

    /* Rate limiting state */
    uint64_t last_request_time;
    uint32_t requests_this_minute;
    uint64_t minute_start_time;

    /* Statistics */
    uint64_t total_requests;
    uint64_t successful_requests;
    uint64_t failed_requests;
    float avg_response_time_ms;
};

/** @brief Typedef for creative_api_client */
typedef struct creative_api_client creative_api_client_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create API client
 *
 * @param config Configuration
 * @return Client or NULL on error
 */
creative_api_client_t* creative_api_client_create(
    const creative_api_client_config_t* config);

/**
 * @brief Destroy API client
 *
 * @param client Client to destroy
 */
void creative_api_client_destroy(creative_api_client_t* client);

/**
 * @brief Test API connection
 *
 * @param client Client
 * @return 0 on success, -1 on error
 */
int creative_api_client_test(creative_api_client_t* client);

//=============================================================================
// Image Generation API
//=============================================================================

/**
 * @brief Generate image via API
 *
 * @param client Client
 * @param request Request
 * @param response Output response
 * @return 0 on success, -1 on error
 */
int api_generate_image(creative_api_client_t* client,
                        const api_image_request_t* request,
                        api_image_response_t* response);

/**
 * @brief Generate image async
 *
 * @param client Client
 * @param request Request
 * @param callback Completion callback
 * @param user_data User data for callback
 * @return 0 on success, -1 on error
 */
typedef void (*api_image_callback_t)(const api_image_response_t* response,
                                      void* user_data);

int api_generate_image_async(creative_api_client_t* client,
                              const api_image_request_t* request,
                              api_image_callback_t callback,
                              void* user_data);

/**
 * @brief Upscale image via API
 *
 * @param client Client
 * @param image Input image
 * @param scale Scale factor (2 or 4)
 * @param response Output response
 * @return 0 on success, -1 on error
 */
int api_upscale_image(creative_api_client_t* client,
                       const visual_image_t* image,
                       uint32_t scale,
                       api_image_response_t* response);

//=============================================================================
// Text Generation API
//=============================================================================

/**
 * @brief Generate text via API
 *
 * @param client Client
 * @param request Request
 * @param response Output response
 * @return 0 on success, -1 on error
 */
int api_generate_text(creative_api_client_t* client,
                       const api_text_request_t* request,
                       api_text_response_t* response);

/**
 * @brief Generate text streaming
 *
 * @param client Client
 * @param request Request
 * @param token_callback Called for each token
 * @param complete_callback Called when complete
 * @param user_data User data for callbacks
 * @return 0 on success, -1 on error
 */
typedef void (*api_token_callback_t)(const char* token, void* user_data);
typedef void (*api_complete_callback_t)(const api_text_response_t* response,
                                         void* user_data);

int api_generate_text_stream(creative_api_client_t* client,
                              const api_text_request_t* request,
                              api_token_callback_t token_callback,
                              api_complete_callback_t complete_callback,
                              void* user_data);

//=============================================================================
// Provider-Specific API
//=============================================================================

/**
 * @brief List available models for provider
 *
 * @param client Client
 * @param models Output array of model names (caller allocates)
 * @param max_models Max models to return
 * @return Number of models
 */
uint32_t api_list_models(creative_api_client_t* client,
                          char** models,
                          uint32_t max_models);

/**
 * @brief Get provider capabilities
 *
 * @param client Client
 * @return Capability flags
 */
uint32_t api_get_capabilities(const creative_api_client_t* client);

/**
 * @brief Get account balance/credits
 *
 * @param client Client
 * @param balance Output balance
 * @param currency Output currency string
 * @return 0 on success, -1 on error
 */
int api_get_balance(creative_api_client_t* client,
                     float* balance,
                     char* currency);

//=============================================================================
// Utility API
//=============================================================================

/**
 * @brief Check if rate limited
 *
 * @param client Client
 * @return true if currently rate limited
 */
bool api_is_rate_limited(const creative_api_client_t* client);

/**
 * @brief Get time until rate limit resets
 *
 * @param client Client
 * @return Milliseconds until reset, 0 if not limited
 */
uint32_t api_rate_limit_reset_ms(const creative_api_client_t* client);

/**
 * @brief Get status string
 *
 * @param status Status code
 * @return Status string
 */
const char* api_status_string(api_status_t status);

/**
 * @brief Free image response
 *
 * @param response Response to free
 */
void api_image_response_free(api_image_response_t* response);

/**
 * @brief Free text response
 *
 * @param response Response to free
 */
void api_text_response_free(api_text_response_t* response);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CREATIVE_API_CLIENT_H */
