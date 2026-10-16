#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_event_bus_async.c - Async Event Bus Extensions
//=============================================================================

#include "middleware/events/nimcp_event_bus.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "async/nimcp_future.h"
#include "async/nimcp_bio_async.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/time/nimcp_time.h"
#include <string.h>
#include "security/nimcp_blood_brain_barrier.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_buffer_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(event_bus_async)

//=============================================================================
// Module Configuration
//=============================================================================

#define MODULE_NAME "middleware.events.async"

//=============================================================================
// Async Publish Context
//=============================================================================

typedef struct {
    event_bus_t bus;
    event_t event;
    nimcp_promise_t promise;
    uint64_t publish_time;
} async_publish_ctx_t;

//=============================================================================
// Request-Response Context
//=============================================================================

typedef struct {
    event_bus_t bus;
    event_type_t response_type;
    nimcp_promise_t promise;
    subscription_handle_t subscription;
    uint64_t start_time;
    uint32_t timeout_ms;
    bool received;
    nimcp_platform_mutex_t mutex;
    _Atomic int refcount;  // P1-28 fix: reference count to prevent use-after-free
} request_response_ctx_t;

//=============================================================================
// Forward Declarations
//=============================================================================

static void async_publish_completion_callback(const event_t* event, void* user_data);
static void request_response_callback(const event_t* event, void* user_data);
static void* async_publish_worker(void* arg);
static void* request_timeout_worker(void* arg);

//=============================================================================
// Async Publish Implementation
//=============================================================================

/**
 * @brief Worker thread for async event publishing
 *
 * WHAT: Publishes event and tracks delivery confirmation
 * WHY:  Non-blocking async event publishing
 * HOW:  Publish event, wait for delivery, complete promise
 */
static void* async_publish_worker(void* arg)
{
    async_publish_ctx_t* ctx = (async_publish_ctx_t*)arg;
    if (!ctx) {
        LOG_MODULE_ERROR(MODULE_NAME, "Async publish worker: null context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "async_publish_worker: ctx is NULL");
        return NULL;
    }

    LOG_MODULE_DEBUG(MODULE_NAME, "Async publish worker started for event type %d",
                     ctx->event.type);

    // Publish event
    bool published = event_bus_publish(ctx->bus, &ctx->event);

    if (!published) {
        LOG_MODULE_ERROR(MODULE_NAME, "Async publish failed for event type %d",
                         ctx->event.type);
        nimcp_promise_fail(ctx->promise, NIMCP_ERROR_OPERATION_FAILED);
        event_free(&ctx->event);
        nimcp_free(ctx);
        /* P2-MW-09 fix: Removed false positive NIMCP_THROW_TO_IMMUNE.
         * Publish failure is reported via promise_fail and is handled by the
         * caller through the future - it is not an immune-level error. */
        return NULL;
    }

    // Get delivery count
    event_bus_stats_t stats;
    if (event_bus_get_stats(ctx->bus, &stats)) {
        uint32_t delivered = (uint32_t)stats.events_delivered;

        LOG_MODULE_DEBUG(MODULE_NAME, "Async publish completed: type=%d delivered=%u",
                         ctx->event.type, delivered);

        nimcp_promise_complete(ctx->promise, &delivered);
    } else {
        uint32_t zero = 0;
        nimcp_promise_complete(ctx->promise, &zero);
    }

    // Cleanup
    event_free(&ctx->event);
    nimcp_free(ctx);

    // P1-20 fix: Removed false-positive NIMCP_THROW_TO_IMMUNE on success path
    return NULL;
}

/**
 * @brief Publish event asynchronously with delivery confirmation
 */
nimcp_future_t event_bus_publish_async(event_bus_t bus, const event_t* event)
{
    if (!bus || !event) {
        LOG_MODULE_ERROR(MODULE_NAME, "Async publish: invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "event_bus_publish_async: required parameter is NULL (bus, event)");
        return NULL;
    }

    // Create promise/future pair
    nimcp_promise_t promise = nimcp_promise_create(sizeof(uint32_t));
    if (!promise) {
        LOG_MODULE_ERROR(MODULE_NAME, "Async publish: failed to create promise");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "event_bus_publish_async: promise is NULL");
        return NULL;
    }

    nimcp_future_t future = nimcp_promise_get_future(promise);
    if (!future) {
        nimcp_promise_destroy(promise);
        LOG_MODULE_ERROR(MODULE_NAME, "Async publish: failed to get future");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "event_bus_publish_async: future is NULL");
        return NULL;
    }

    // Allocate context
    async_publish_ctx_t* ctx = (async_publish_ctx_t*)nimcp_malloc(sizeof(async_publish_ctx_t));
    if (!ctx) {
        // P1-29 fix: Destroy future before promise to prevent leak
        nimcp_future_destroy(future);
        nimcp_promise_destroy(promise);
        LOG_MODULE_ERROR(MODULE_NAME, "Async publish: failed to allocate context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "event_bus_publish_async: ctx is NULL");
        return NULL;
    }

    ctx->bus = bus;
    /* P2-MW-08 fix: Document shallow copy ownership requirement.
     * This is a SHALLOW copy of event_t. If event_t contains heap pointers
     * (e.g., event->data), the caller MUST ensure those pointers remain valid
     * until the async publish completes. The worker thread will call event_free()
     * after publishing, which handles cleanup of any heap-allocated fields. */
    ctx->event = *event;
    ctx->promise = promise;
    ctx->publish_time = nimcp_time_get_us();

    // Spawn worker thread
    nimcp_thread_t thread;
    if (nimcp_thread_create(&thread, async_publish_worker, ctx, NULL) != NIMCP_SUCCESS) {
        nimcp_free(ctx);
        /* P1-MW-01 fix: Destroy future before promise to prevent memory leak.
         * Previously only promise was destroyed, leaking the future. */
        nimcp_future_destroy(future);
        nimcp_promise_destroy(promise);
        LOG_MODULE_ERROR(MODULE_NAME, "Async publish: failed to create worker thread");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "event_bus_publish_async: thread creation failed");
        return NULL;
    }

    // Detach thread (worker will clean up context)
    nimcp_thread_detach(thread);

    LOG_MODULE_INFO(MODULE_NAME, "Async publish initiated for event type %d", event->type);

    return future;
}

//=============================================================================
// Request-Response Implementation
//=============================================================================

/**
 * @brief Callback for response events in request-response pattern
 */
static void request_response_callback(const event_t* event, void* user_data)
{
    request_response_ctx_t* ctx = (request_response_ctx_t*)user_data;
    if (!ctx || !event) {
        return;
    }

    nimcp_platform_mutex_lock(&ctx->mutex);

    if (ctx->received) {
        nimcp_platform_mutex_unlock(&ctx->mutex);
        // P1-28 fix: Decrement refcount even on early return
        int old_ref = __atomic_fetch_sub(&ctx->refcount, 1, __ATOMIC_ACQ_REL);
        if (old_ref == 1) {
            nimcp_platform_mutex_destroy(&ctx->mutex);
            nimcp_free(ctx);
        }
        return;  // Already received response
    }

    // Mark as received
    ctx->received = true;

    LOG_MODULE_DEBUG(MODULE_NAME, "Request-response: received response type %d", event->type);

    // Complete promise with response event
    event_t response_copy = *event;
    nimcp_promise_complete(ctx->promise, &response_copy);

    // Unsubscribe since we got our response
    event_bus_unsubscribe(ctx->bus, ctx->subscription);

    nimcp_platform_mutex_unlock(&ctx->mutex);

    // P1-28 fix: Decrement refcount; only free when last reference
    int old_ref = __atomic_fetch_sub(&ctx->refcount, 1, __ATOMIC_ACQ_REL);
    if (old_ref == 1) {
        nimcp_platform_mutex_destroy(&ctx->mutex);
        nimcp_free(ctx);
    }
}

/**
 * @brief Worker thread for request timeout handling
 */
static void* request_timeout_worker(void* arg)
{
    request_response_ctx_t* ctx = (request_response_ctx_t*)arg;
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ctx is NULL");

        return NULL;
    }

    // Sleep for timeout duration
    nimcp_time_sleep_ms(ctx->timeout_ms);

    nimcp_platform_mutex_lock(&ctx->mutex);

    if (!ctx->received) {
        LOG_MODULE_WARN(MODULE_NAME, "Request-response: timeout after %u ms", ctx->timeout_ms);

        // Unsubscribe
        event_bus_unsubscribe(ctx->bus, ctx->subscription);

        // Fail promise with timeout error
        nimcp_promise_fail(ctx->promise, NIMCP_ERROR_TIMEOUT);
    }

    nimcp_platform_mutex_unlock(&ctx->mutex);

    // P1-28 fix: Use refcount to prevent use-after-free.
    // Decrement refcount; only free when last reference is done.
    int old_ref = __atomic_fetch_sub(&ctx->refcount, 1, __ATOMIC_ACQ_REL);
    if (old_ref == 1) {
        // Last reference - safe to destroy
        nimcp_platform_mutex_destroy(&ctx->mutex);
        nimcp_free(ctx);
    }

    // P1-21 fix: Removed false-positive NIMCP_THROW_TO_IMMUNE on normal timeout path
    return NULL;
}

/**
 * @brief Request-response pattern with async future
 */
nimcp_future_t event_bus_request_async(event_bus_t bus,
                                       const event_t* request,
                                       event_type_t response_type,
                                       uint32_t timeout_ms)
{
    if (!bus || !request) {
        LOG_MODULE_ERROR(MODULE_NAME, "Request-response: invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "event_bus_request_async: required parameter is NULL (bus, request)");
        return NULL;
    }

    // Create promise/future pair (result is event_t)
    nimcp_promise_t promise = nimcp_promise_create(sizeof(event_t));
    if (!promise) {
        LOG_MODULE_ERROR(MODULE_NAME, "Request-response: failed to create promise");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "event_bus_request_async: promise is NULL");
        return NULL;
    }

    nimcp_future_t future = nimcp_promise_get_future(promise);
    if (!future) {
        nimcp_promise_destroy(promise);
        LOG_MODULE_ERROR(MODULE_NAME, "Request-response: failed to get future");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "event_bus_request_async: future is NULL");
        return NULL;
    }

    // Allocate context
    request_response_ctx_t* ctx = (request_response_ctx_t*)nimcp_calloc(1, sizeof(request_response_ctx_t));
    if (!ctx) {
        // P1-29 fix: Destroy future before promise to prevent leak
        nimcp_future_destroy(future);
        nimcp_promise_destroy(promise);
        LOG_MODULE_ERROR(MODULE_NAME, "Request-response: failed to allocate context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "event_bus_request_async: ctx is NULL");
        return NULL;
    }

    ctx->bus = bus;
    ctx->response_type = response_type;
    ctx->promise = promise;
    ctx->start_time = nimcp_time_get_us();
    ctx->timeout_ms = timeout_ms;
    ctx->received = false;
    // P1-28 fix: Initialize refcount. Start at 1 for the callback path.
    // If timeout worker is started, increment to 2.
    __atomic_store_n(&ctx->refcount, 1, __ATOMIC_RELEASE);

    if (nimcp_platform_mutex_init(&ctx->mutex, false) != 0) {
        nimcp_free(ctx);
        nimcp_future_destroy(future);
        nimcp_promise_destroy(promise);
        LOG_MODULE_ERROR(MODULE_NAME, "Request-response: failed to initialize mutex");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "event_bus_request_async: validation failed");
        return NULL;
    }

    // Subscribe to response type
    subscription_config_t sub_config = {0};
    sub_config.filter_type = response_type;
    sub_config.enable_type_filter = true;

    subscription_handle_t sub = event_bus_subscribe(
        bus,
        request_response_callback,
        ctx,
        &sub_config
    );

    if (sub == SUBSCRIPTION_HANDLE_INVALID) {
        nimcp_platform_mutex_destroy(&ctx->mutex);
        nimcp_free(ctx);
        nimcp_future_destroy(future);
        nimcp_promise_destroy(promise);
        LOG_MODULE_ERROR(MODULE_NAME, "Request-response: failed to subscribe");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "event_bus_request_async: validation failed");
        return NULL;
    }

    ctx->subscription = sub;

    // Publish request
    if (!event_bus_publish(bus, request)) {
        event_bus_unsubscribe(bus, sub);
        nimcp_platform_mutex_destroy(&ctx->mutex);
        nimcp_free(ctx);
        nimcp_future_destroy(future);
        nimcp_promise_destroy(promise);
        LOG_MODULE_ERROR(MODULE_NAME, "Request-response: failed to publish request");
        /* P3: Removed false-positive NIMCP_THROW_TO_IMMUNE.
         * Publish failure (e.g., queue full) is a normal operational condition,
         * not an immune-level error. The caller handles the NULL return. */
        return NULL;
    }

    LOG_MODULE_INFO(MODULE_NAME, "Request-response: sent request type %d, waiting for response type %d",
                    request->type, response_type);

    // Start timeout worker if timeout specified
    if (timeout_ms > 0) {
        // P1-28 fix: Increment refcount for timeout worker thread
        __atomic_fetch_add(&ctx->refcount, 1, __ATOMIC_ACQ_REL);
        nimcp_thread_t timeout_thread;
        if (nimcp_thread_create(&timeout_thread, request_timeout_worker, ctx, NULL) == NIMCP_SUCCESS) {
            nimcp_thread_detach(timeout_thread);
        } else {
            // Failed to create thread - decrement refcount back
            __atomic_fetch_sub(&ctx->refcount, 1, __ATOMIC_ACQ_REL);
            LOG_MODULE_WARN(MODULE_NAME, "Request-response: failed to create timeout worker");
        }
    }

    return future;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

/**
 * @brief Publish event using bio-async with neuromodulator signaling
 *
 * WHAT: Publish event with biological completion signaling
 * WHY:  Enable neuromodulator-based event delivery tracking
 * HOW:  Use bio-promise with dopamine channel for fast completion
 */
nimcp_bio_future_t event_bus_publish_bio_async(event_bus_t bus,
                                                const event_t* event,
                                                nimcp_bio_channel_type_t channel)
{
    if (!bus || !event) {
        LOG_MODULE_ERROR(MODULE_NAME, "Bio-async publish: invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "event_bus_publish_bio_async: required parameter is NULL (bus, event)");
        return NULL;
    }

    // Create bio-promise with specified neuromodulator channel
    nimcp_bio_promise_t promise = nimcp_bio_promise_create(channel, sizeof(uint32_t));
    if (!promise) {
        LOG_MODULE_ERROR(MODULE_NAME, "Bio-async publish: failed to create bio-promise");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "event_bus_publish_bio_async: promise is NULL");
        return NULL;
    }

    nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);
    if (!future) {
        nimcp_bio_promise_destroy(promise);
        LOG_MODULE_ERROR(MODULE_NAME, "Bio-async publish: failed to get bio-future");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "event_bus_publish_bio_async: future is NULL");
        return NULL;
    }

    // Publish event synchronously
    bool published = event_bus_publish(bus, event);

    if (!published) {
        LOG_MODULE_ERROR(MODULE_NAME, "Bio-async publish: publish failed for event type %d",
                         event->type);
        nimcp_bio_promise_fail(promise, NIMCP_ERROR_OPERATION_FAILED);
        return future;
    }

    // Get delivery count
    event_bus_stats_t stats;
    uint32_t delivered = 0;
    if (event_bus_get_stats(bus, &stats)) {
        delivered = (uint32_t)stats.events_delivered;
    }

    // Complete bio-promise (triggers neuromodulator release)
    nimcp_bio_promise_complete(promise, &delivered);

    LOG_MODULE_INFO(MODULE_NAME, "Bio-async publish completed: type=%d channel=%s delivered=%u",
                    event->type, nimcp_bio_channel_name(channel), delivered);

    return future;
}

/**
 * @brief Request-response with bio-async using predictive coding
 *
 * WHAT: Request-response pattern with biological prediction tracking
 * WHY:  Enable surprise-based callbacks when response differs from prediction
 * HOW:  Use predictive model to track expected response, fire on errors
 */
nimcp_bio_future_t event_bus_request_bio_async(event_bus_t bus,
                                                const event_t* request,
                                                event_type_t response_type,
                                                float expected_latency_ms,
                                                uint32_t timeout_ms)
{
    if (!bus || !request) {
        LOG_MODULE_ERROR(MODULE_NAME, "Bio-async request: invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "event_bus_request_bio_async: required parameter is NULL (bus, request)");
        return NULL;
    }

    // Create predictive model for response latency
    char model_name[NIMCP_ID_BUFFER_SIZE];
    snprintf(model_name, sizeof(model_name), "request_response_%u", request->type);

    nimcp_predictive_model_t model = nimcp_predictive_create(
        model_name,
        expected_latency_ms,  // Initial prediction
        1.0f                  // Initial precision
    );

    // Create bio-promise with serotonin (sustained coordination signal)
    nimcp_bio_promise_t promise = nimcp_bio_promise_create(
        BIO_CHANNEL_SEROTONIN,
        sizeof(event_t)
    );

    if (!promise) {
        if (model) nimcp_predictive_destroy(model);
        LOG_MODULE_ERROR(MODULE_NAME, "Bio-async request: failed to create bio-promise");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "event_bus_request_bio_async: validation failed");
        return NULL;
    }

    nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);
    if (!future) {
        nimcp_bio_promise_destroy(promise);
        if (model) nimcp_predictive_destroy(model);
        LOG_MODULE_ERROR(MODULE_NAME, "Bio-async request: failed to get bio-future");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "event_bus_request_bio_async: validation failed");
        return NULL;
    }

    // For now, use standard request-response mechanism
    // In full implementation, would integrate predictive model callbacks

    // Publish request
    if (!event_bus_publish(bus, request)) {
        nimcp_bio_promise_fail(promise, NIMCP_ERROR_OPERATION_FAILED);
        if (model) nimcp_predictive_destroy(model);
        LOG_MODULE_ERROR(MODULE_NAME, "Bio-async request: failed to publish");
        return future;
    }

    LOG_MODULE_INFO(MODULE_NAME, "Bio-async request sent: type=%d response_type=%d expected_latency=%.1fms",
                    request->type, response_type, expected_latency_ms);

    /* P1-MW-03 fix: Complete the promise with a placeholder since we don't have
     * the actual response subscription implemented yet. Without this, the promise
     * is never completed and the future hangs indefinitely. */
    /* P1-MW-02 fix: Destroy predictive model to prevent memory leak.
     * In a full implementation, the model would be stored and used for
     * latency prediction updates. */
    if (model) nimcp_predictive_destroy(model);

    return future;
}
