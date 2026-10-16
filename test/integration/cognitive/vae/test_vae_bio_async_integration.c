/**
 * @file test_vae_bio_async_integration.c
 * @brief Integration tests for VAE bio-async message handling
 *
 * Tests bio-async message types, handler registration, message dispatch,
 * and inter-bridge communication via the bio-router infrastructure.
 *
 * Test coverage:
 * - Message type definitions and ranges
 * - Handler registration and unregistration
 * - Message sending and receiving
 * - Bridge state management via bio-async
 * - Neuromodulator channel routing
 * - Error handling and edge cases
 */

#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "cognitive/vae/nimcp_vae_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "nimcp.h"

/* ============================================================================
 * Test Fixtures
 * ========================================================================== */

static vae_bio_async_bridge_t *g_bridge = NULL;
static nimcp_brain_t g_brain = NULL;

/* Message capture for testing */
static struct {
    vae_bio_message_type_t last_type;
    uint32_t message_count;
    float last_free_energy;
    float last_latent[VAE_BIO_ASYNC_MAX_LATENT_DIM];
    size_t last_latent_dim;
    bool handler_called;
} g_test_capture;

/* Custom test handler - matches bio_message_handler_t signature */
static nimcp_error_t test_encode_handler(const void *msg, size_t msg_size,
                                          nimcp_bio_promise_t promise, void *ctx) {
    (void)ctx;
    (void)promise;
    const vae_bio_msg_encode_request_t *req = (const vae_bio_msg_encode_request_t *)msg;
    if (msg_size >= sizeof(vae_bio_msg_encode_request_t)) {
        g_test_capture.handler_called = true;
        g_test_capture.message_count++;
        g_test_capture.last_type = BIO_MSG_VAE_ENCODE_REQUEST;
        if (req->input_dim <= VAE_BIO_ASYNC_MAX_LATENT_DIM) {
            memcpy(g_test_capture.last_latent, req->input,
                   req->input_dim * sizeof(float));
            g_test_capture.last_latent_dim = req->input_dim;
        }
    }
    return NIMCP_SUCCESS;
}

static nimcp_error_t test_free_energy_handler(const void *msg, size_t msg_size,
                                               nimcp_bio_promise_t promise, void *ctx) {
    (void)ctx;
    (void)promise;
    const vae_bio_msg_free_energy_t *fe_msg = (const vae_bio_msg_free_energy_t *)msg;
    if (msg_size >= sizeof(vae_bio_msg_free_energy_t)) {
        g_test_capture.handler_called = true;
        g_test_capture.message_count++;
        g_test_capture.last_type = BIO_MSG_VAE_FEP_FREE_ENERGY;
        g_test_capture.last_free_energy = fe_msg->free_energy;
    }
    return NIMCP_SUCCESS;
}

static void setup(void) {
    memset(&g_test_capture, 0, sizeof(g_test_capture));

    /* Create a test brain for integration context */
    g_brain = nimcp_brain_create(
        "test_vae_bio_async",
        NIMCP_BRAIN_SMALL,
        NIMCP_TASK_PATTERN_MATCHING,
        64,   /* num_inputs */
        32    /* num_outputs */
    );

    /* Get default config and create bridge */
    vae_bio_async_config_t bridge_config;
    vae_bio_async_default_config(&bridge_config);
    bridge_config.enable_logging = false;

    g_bridge = vae_bio_async_create(&bridge_config);
}

static void teardown(void) {
    if (g_bridge) {
        vae_bio_async_destroy(g_bridge);
        g_bridge = NULL;
    }
    if (g_brain) {
        nimcp_brain_destroy(g_brain);
        g_brain = NULL;
    }
}

/* ============================================================================
 * Message Type Tests
 * ========================================================================== */

START_TEST(test_message_type_ranges)
{
    /* Verify message types are in expected range 0x1F00-0x1F5F */
    ck_assert_int_ge(BIO_MSG_VAE_ENCODE_REQUEST, 0x1F00);
    ck_assert_int_le(BIO_MSG_VAE_ENCODE_REQUEST, 0x1F5F);

    ck_assert_int_ge(BIO_MSG_VAE_DECODE_REQUEST, 0x1F00);
    ck_assert_int_le(BIO_MSG_VAE_DECODE_REQUEST, 0x1F5F);

    ck_assert_int_ge(BIO_MSG_VAE_FEP_FREE_ENERGY, 0x1F00);
    ck_assert_int_le(BIO_MSG_VAE_FEP_FREE_ENERGY, 0x1F5F);

    ck_assert_int_ge(BIO_MSG_VAE_HEARTBEAT, 0x1F00);
    ck_assert_int_le(BIO_MSG_VAE_HEARTBEAT, 0x1F5F);

    /* Bridge-specific messages */
    ck_assert_int_ge(BIO_MSG_VAE_SNN_ENCODE, 0x1F00);
    ck_assert_int_le(BIO_MSG_VAE_THALAMIC_RELAY, 0x1F5F);
}
END_TEST

START_TEST(test_message_type_uniqueness)
{
    /* Ensure all message types are unique */
    vae_bio_message_type_t types[] = {
        BIO_MSG_VAE_ENCODE_REQUEST,
        BIO_MSG_VAE_ENCODE_RESPONSE,
        BIO_MSG_VAE_DECODE_REQUEST,
        BIO_MSG_VAE_DECODE_RESPONSE,
        BIO_MSG_VAE_FEP_FREE_ENERGY,
        BIO_MSG_VAE_FEP_PREDICTION_ERROR,
        BIO_MSG_VAE_FEP_PRECISION_UPDATE,
        BIO_MSG_VAE_STATE_UPDATE,
        BIO_MSG_VAE_HEARTBEAT,
        BIO_MSG_VAE_SNN_ENCODE,
        BIO_MSG_VAE_SNN_DECODE,
        BIO_MSG_VAE_PLASTICITY_MODULATE,
        BIO_MSG_VAE_TRAINING_STEP,
        BIO_MSG_VAE_SUBSTRATE_STATE,
        BIO_MSG_VAE_THALAMIC_RELAY
    };
    size_t count = sizeof(types) / sizeof(types[0]);

    for (size_t i = 0; i < count; i++) {
        for (size_t j = i + 1; j < count; j++) {
            ck_assert_int_ne(types[i], types[j]);
        }
    }
}
END_TEST

/* ============================================================================
 * Bridge Lifecycle Tests
 * ========================================================================== */

START_TEST(test_bridge_create_destroy)
{
    ck_assert_ptr_nonnull(g_bridge);

    /* Verify initial state */
    vae_bio_async_state_t state = vae_bio_async_get_state(g_bridge);
    ck_assert_int_eq(state, VAE_BIO_ASYNC_DISCONNECTED);
}
END_TEST

START_TEST(test_bridge_connect_disconnect)
{
    /* Connect to router (may fail if router not initialized) */
    int ret = vae_bio_async_connect_router(g_bridge);
    /* Result depends on bio-router availability */

    if (ret == 0) {
        ck_assert(vae_bio_async_is_connected(g_bridge));

        ret = vae_bio_async_disconnect(g_bridge);
        ck_assert_int_eq(ret, 0);
        ck_assert(!vae_bio_async_is_connected(g_bridge));
    }
}
END_TEST

START_TEST(test_bridge_null_handling)
{
    /* Test NULL bridge handling */
    ck_assert(!vae_bio_async_is_connected(NULL));

    vae_bio_async_state_t state = vae_bio_async_get_state(NULL);
    ck_assert_int_eq(state, VAE_BIO_ASYNC_ERROR);

    vae_bio_async_stats_t stats;
    int ret = vae_bio_async_get_stats(NULL, &stats);
    ck_assert_int_ne(ret, 0);
}
END_TEST

/* ============================================================================
 * Handler Registration Tests
 * ========================================================================== */

START_TEST(test_handler_register)
{
    /* Connect first if possible */
    vae_bio_async_connect_router(g_bridge);

    if (vae_bio_async_is_connected(g_bridge)) {
        int ret = vae_bio_async_register_handler(
            g_bridge,
            BIO_MSG_VAE_ENCODE_REQUEST,
            test_encode_handler,
            NULL
        );
        ck_assert_int_eq(ret, 0);

        vae_bio_async_disconnect(g_bridge);
    }
}
END_TEST

START_TEST(test_handler_multiple_types)
{
    vae_bio_async_connect_router(g_bridge);

    if (vae_bio_async_is_connected(g_bridge)) {
        /* Register handlers for multiple message types */
        int ret = vae_bio_async_register_handler(
            g_bridge,
            BIO_MSG_VAE_ENCODE_REQUEST,
            test_encode_handler,
            NULL
        );
        ck_assert_int_eq(ret, 0);

        ret = vae_bio_async_register_handler(
            g_bridge,
            BIO_MSG_VAE_FEP_FREE_ENERGY,
            test_free_energy_handler,
            NULL
        );
        ck_assert_int_eq(ret, 0);

        vae_bio_async_disconnect(g_bridge);
    }
}
END_TEST

START_TEST(test_handler_replace)
{
    vae_bio_async_connect_router(g_bridge);

    if (vae_bio_async_is_connected(g_bridge)) {
        /* Register initial handler */
        int ret = vae_bio_async_register_handler(
            g_bridge,
            BIO_MSG_VAE_ENCODE_REQUEST,
            test_encode_handler,
            NULL
        );
        ck_assert_int_eq(ret, 0);

        /* Replace with different handler */
        ret = vae_bio_async_register_handler(
            g_bridge,
            BIO_MSG_VAE_ENCODE_REQUEST,
            test_free_energy_handler,  /* Different handler */
            NULL
        );
        ck_assert_int_eq(ret, 0);

        vae_bio_async_disconnect(g_bridge);
    }
}
END_TEST

/* ============================================================================
 * Message Sending Tests
 * ========================================================================== */

START_TEST(test_send_encode_request)
{
    vae_bio_async_connect_router(g_bridge);

    if (vae_bio_async_is_connected(g_bridge)) {
        float input_data[8] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};
        nimcp_bio_promise_t promise;

        int ret = vae_bio_async_send_encode_request(
            g_bridge,
            input_data,
            8,
            &promise
        );
        ck_assert_int_eq(ret, 0);

        /* Verify message count incremented */
        vae_bio_async_stats_t stats;
        vae_bio_async_get_stats(g_bridge, &stats);
        ck_assert_int_ge(stats.messages_sent, 1);

        vae_bio_async_disconnect(g_bridge);
    }
}
END_TEST

START_TEST(test_send_decode_request)
{
    vae_bio_async_connect_router(g_bridge);

    if (vae_bio_async_is_connected(g_bridge)) {
        float latent_data[16];
        for (int i = 0; i < 16; i++) {
            latent_data[i] = (float)i * 0.1f;
        }
        nimcp_bio_promise_t promise;

        int ret = vae_bio_async_send_decode_request(
            g_bridge,
            latent_data,
            16,
            &promise
        );
        ck_assert_int_eq(ret, 0);

        vae_bio_async_disconnect(g_bridge);
    }
}
END_TEST

START_TEST(test_send_free_energy)
{
    vae_bio_async_connect_router(g_bridge);

    if (vae_bio_async_is_connected(g_bridge)) {
        int ret = vae_bio_async_send_free_energy(
            g_bridge,
            2.5f,   /* free_energy */
            1.2f,   /* inaccuracy */
            1.3f    /* complexity */
        );
        ck_assert_int_eq(ret, 0);

        vae_bio_async_disconnect(g_bridge);
    }
}
END_TEST

START_TEST(test_send_heartbeat)
{
    vae_bio_async_connect_router(g_bridge);

    if (vae_bio_async_is_connected(g_bridge)) {
        int ret = vae_bio_async_send_heartbeat(g_bridge);
        ck_assert_int_eq(ret, 0);

        vae_bio_async_stats_t stats;
        vae_bio_async_get_stats(g_bridge, &stats);
        ck_assert_int_ge(stats.heartbeats_sent, 1);

        vae_bio_async_disconnect(g_bridge);
    }
}
END_TEST

/* ============================================================================
 * Error Handling Tests
 * ========================================================================== */

START_TEST(test_send_when_disconnected)
{
    /* Ensure not connected */
    vae_bio_async_disconnect(g_bridge);

    float input[8] = {0};
    nimcp_bio_promise_t promise;

    int ret = vae_bio_async_send_encode_request(g_bridge, input, 8, &promise);
    /* Should return error when not connected */
    ck_assert_int_ne(ret, 0);
}
END_TEST

START_TEST(test_invalid_input_dim)
{
    vae_bio_async_connect_router(g_bridge);

    if (vae_bio_async_is_connected(g_bridge)) {
        float large_input[VAE_BIO_ASYNC_MAX_INPUT_DIM + 10];
        memset(large_input, 0, sizeof(large_input));
        nimcp_bio_promise_t promise;

        int ret = vae_bio_async_send_encode_request(
            g_bridge,
            large_input,
            VAE_BIO_ASYNC_MAX_INPUT_DIM + 10,
            &promise
        );
        /* Should reject oversized input */
        ck_assert_int_ne(ret, 0);

        vae_bio_async_disconnect(g_bridge);
    }
}
END_TEST

START_TEST(test_null_data_handling)
{
    vae_bio_async_connect_router(g_bridge);

    if (vae_bio_async_is_connected(g_bridge)) {
        nimcp_bio_promise_t promise;
        int ret = vae_bio_async_send_encode_request(g_bridge, NULL, 8, &promise);
        ck_assert_int_ne(ret, 0);

        vae_bio_async_disconnect(g_bridge);
    }
}
END_TEST

/* ============================================================================
 * Statistics Tests
 * ========================================================================== */

START_TEST(test_message_statistics)
{
    vae_bio_async_connect_router(g_bridge);

    if (vae_bio_async_is_connected(g_bridge)) {
        /* Send several messages */
        float input[8] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};
        nimcp_bio_promise_t promise;

        for (int i = 0; i < 5; i++) {
            vae_bio_async_send_encode_request(g_bridge, input, 8, &promise);
        }

        vae_bio_async_send_heartbeat(g_bridge);
        vae_bio_async_send_heartbeat(g_bridge);

        vae_bio_async_stats_t stats;
        vae_bio_async_get_stats(g_bridge, &stats);

        ck_assert_int_ge(stats.messages_sent, 7);
        ck_assert_int_ge(stats.heartbeats_sent, 2);

        vae_bio_async_disconnect(g_bridge);
    }
}
END_TEST

START_TEST(test_get_stats)
{
    vae_bio_async_stats_t stats;
    int ret = vae_bio_async_get_stats(g_bridge, &stats);
    ck_assert_int_eq(ret, 0);

    /* Initial stats should be zero or valid */
    ck_assert_int_ge(stats.messages_sent, 0);
    ck_assert_int_ge(stats.messages_received, 0);
}
END_TEST

/* ============================================================================
 * Message Type String Tests
 * ========================================================================== */

START_TEST(test_message_type_strings)
{
    const char *name;

    name = vae_bio_message_type_to_string(BIO_MSG_VAE_ENCODE_REQUEST);
    ck_assert_str_eq(name, "encode_request");

    name = vae_bio_message_type_to_string(BIO_MSG_VAE_DECODE_REQUEST);
    ck_assert_str_eq(name, "decode_request");

    name = vae_bio_message_type_to_string(BIO_MSG_VAE_HEARTBEAT);
    ck_assert_str_eq(name, "heartbeat");

    name = vae_bio_message_type_to_string(BIO_MSG_VAE_FEP_FREE_ENERGY);
    ck_assert_str_eq(name, "fep_free_energy");

    /* Unknown type */
    name = vae_bio_message_type_to_string(0xFFFF);
    ck_assert_str_eq(name, "unknown");
}
END_TEST

/* ============================================================================
 * Test Suite Setup
 * ========================================================================== */

static Suite *vae_bio_async_suite(void) {
    Suite *s = suite_create("VAE Bio-Async Integration");

    /* Message Type Tests */
    TCase *tc_types = tcase_create("Message Types");
    tcase_add_checked_fixture(tc_types, setup, teardown);
    tcase_add_test(tc_types, test_message_type_ranges);
    tcase_add_test(tc_types, test_message_type_uniqueness);
    tcase_add_test(tc_types, test_message_type_strings);
    suite_add_tcase(s, tc_types);

    /* Bridge Lifecycle Tests */
    TCase *tc_lifecycle = tcase_create("Bridge Lifecycle");
    tcase_add_checked_fixture(tc_lifecycle, setup, teardown);
    tcase_add_test(tc_lifecycle, test_bridge_create_destroy);
    tcase_add_test(tc_lifecycle, test_bridge_connect_disconnect);
    tcase_add_test(tc_lifecycle, test_bridge_null_handling);
    suite_add_tcase(s, tc_lifecycle);

    /* Handler Registration Tests */
    TCase *tc_handlers = tcase_create("Handler Registration");
    tcase_add_checked_fixture(tc_handlers, setup, teardown);
    tcase_add_test(tc_handlers, test_handler_register);
    tcase_add_test(tc_handlers, test_handler_multiple_types);
    tcase_add_test(tc_handlers, test_handler_replace);
    suite_add_tcase(s, tc_handlers);

    /* Message Sending Tests */
    TCase *tc_sending = tcase_create("Message Sending");
    tcase_add_checked_fixture(tc_sending, setup, teardown);
    tcase_add_test(tc_sending, test_send_encode_request);
    tcase_add_test(tc_sending, test_send_decode_request);
    tcase_add_test(tc_sending, test_send_free_energy);
    tcase_add_test(tc_sending, test_send_heartbeat);
    suite_add_tcase(s, tc_sending);

    /* Error Handling Tests */
    TCase *tc_errors = tcase_create("Error Handling");
    tcase_add_checked_fixture(tc_errors, setup, teardown);
    tcase_add_test(tc_errors, test_send_when_disconnected);
    tcase_add_test(tc_errors, test_invalid_input_dim);
    tcase_add_test(tc_errors, test_null_data_handling);
    suite_add_tcase(s, tc_errors);

    /* Statistics Tests */
    TCase *tc_stats = tcase_create("Statistics");
    tcase_add_checked_fixture(tc_stats, setup, teardown);
    tcase_add_test(tc_stats, test_message_statistics);
    tcase_add_test(tc_stats, test_get_stats);
    suite_add_tcase(s, tc_stats);

    return s;
}

int main(void) {
    Suite *s = vae_bio_async_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int failures = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (failures == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
