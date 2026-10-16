/**
 * @file policy_engine_demo.c
 * @brief Demonstration of NIMCP Policy Engine
 *
 * This example shows how to:
 * 1. Create a policy engine
 * 2. Load policies from text and files
 * 3. Create evaluation contexts
 * 4. Evaluate policies
 * 5. Register custom functions
 * 6. Handle policy results
 */

#include <stdio.h>
#include <stdlib.h>
#include "security/nimcp_policy_engine.h"

/* Custom function example */
static nimcp_error_t is_safe_user(
    const nimcp_policy_value_t* args,
    size_t num_args,
    nimcp_policy_value_t* result,
    void* user_data)
{
    if (num_args != 1 || args[0].type != NIMCP_POLICY_VALUE_STRING) {
        return NIMCP_ERROR_INVALID_ARGUMENT;
    }

    /* Simple check: user is safe if username doesn't contain "hacker" */
    result->type = NIMCP_POLICY_VALUE_BOOL;
    result->bool_val = (strstr(args[0].string_val, "hacker") == NULL);

    return NIMCP_OK;
}

/* Event callback example */
static void policy_event_callback(
    const char* event_type,
    const nimcp_policy_result_t* result,
    void* user_data)
{
    printf("[EVENT] %s - Action: %d\n", event_type, result->action);
}

int main(int argc, char** argv) {
    printf("=== NIMCP Policy Engine Demo ===\n\n");

    /* ================================================================
     * 1. Create Policy Engine
     * ================================================================ */
    printf("1. Creating policy engine...\n");

    nimcp_policy_engine_config_t config = {0};
    config.max_policies = 10;
    config.enable_caching = true;
    config.cache_size = 100;
    config.enable_optimization = true;
    config.bio_async = NULL;  /* No bio-async for this demo */

    nimcp_policy_engine_t engine = nimcp_policy_engine_create(&config);
    if (!engine) {
        fprintf(stderr, "Failed to create policy engine\n");
        return 1;
    }
    printf("   ✓ Policy engine created\n\n");

    /* ================================================================
     * 2. Register Custom Function
     * ================================================================ */
    printf("2. Registering custom function...\n");

    nimcp_error_t err = nimcp_policy_register_function(
        engine, "is_safe_user", is_safe_user, NULL
    );
    if (err != NIMCP_OK) {
        fprintf(stderr, "Failed to register custom function\n");
        goto cleanup;
    }
    printf("   ✓ Custom function 'is_safe_user' registered\n\n");

    /* ================================================================
     * 3. Register Event Callback
     * ================================================================ */
    printf("3. Registering event callback...\n");

    err = nimcp_policy_register_callback(engine, policy_event_callback, NULL);
    if (err != NIMCP_OK) {
        fprintf(stderr, "Failed to register callback\n");
        goto cleanup;
    }
    printf("   ✓ Event callback registered\n\n");

    /* ================================================================
     * 4. Load Policies
     * ================================================================ */
    printf("4. Loading policies...\n");

    /* Load SQL injection detection policy */
    const char* sql_policy =
        "rule \"sql_injection\" {\n"
        "    condition: contains(input, \"SELECT\") AND contains(input, \"FROM\")\n"
        "    action: DENY\n"
        "}";

    nimcp_policy_t policy1;
    err = nimcp_policy_engine_load(engine, sql_policy, &policy1);
    if (err != NIMCP_OK) {
        fprintf(stderr, "Failed to load SQL injection policy\n");
        goto cleanup;
    }
    printf("   ✓ SQL injection policy loaded\n");

    /* Load input validation policy */
    const char* validation_policy =
        "rule \"input_length\" {\n"
        "    condition: length(input) > 1000\n"
        "    action: DENY\n"
        "}";

    nimcp_policy_t policy2;
    err = nimcp_policy_engine_load(engine, validation_policy, &policy2);
    if (err != NIMCP_OK) {
        fprintf(stderr, "Failed to load validation policy\n");
        goto cleanup;
    }
    printf("   ✓ Input validation policy loaded\n");

    /* Load user safety policy using custom function */
    const char* user_policy =
        "rule \"user_check\" {\n"
        "    condition: NOT is_safe_user(username)\n"
        "    action: DENY\n"
        "}";

    nimcp_policy_t policy3;
    err = nimcp_policy_engine_load(engine, user_policy, &policy3);
    if (err != NIMCP_OK) {
        fprintf(stderr, "Failed to load user policy\n");
        goto cleanup;
    }
    printf("   ✓ User safety policy loaded\n\n");

    /* ================================================================
     * 5. Test Policy Evaluation
     * ================================================================ */
    printf("5. Testing policy evaluation...\n\n");

    /* Test Case 1: SQL Injection */
    printf("Test Case 1: SQL Injection Detection\n");
    {
        nimcp_policy_context_t ctx = nimcp_policy_context_create();
        nimcp_policy_context_set_string(ctx, "input", "SELECT * FROM users WHERE id=1");
        nimcp_policy_context_set_string(ctx, "username", "alice");

        nimcp_policy_result_t result = {0};
        err = nimcp_policy_evaluate(engine, ctx, &result);

        if (err == NIMCP_OK) {
            printf("   Input: \"SELECT * FROM users WHERE id=1\"\n");
            printf("   Result: %s\n",
                   result.action == NIMCP_POLICY_ACTION_DENY ? "DENIED ✓" : "ALLOWED");
            printf("   Evaluation time: %lu ns\n", result.eval_time_ns);
            nimcp_policy_result_free(&result);
        }

        nimcp_policy_context_destroy(ctx);
    }
    printf("\n");

    /* Test Case 2: Safe Input */
    printf("Test Case 2: Safe Input\n");
    {
        nimcp_policy_context_t ctx = nimcp_policy_context_create();
        nimcp_policy_context_set_string(ctx, "input", "Hello, world!");
        nimcp_policy_context_set_string(ctx, "username", "bob");

        nimcp_policy_result_t result = {0};
        err = nimcp_policy_evaluate(engine, ctx, &result);

        if (err == NIMCP_OK) {
            printf("   Input: \"Hello, world!\"\n");
            printf("   Result: %s\n",
                   result.action == NIMCP_POLICY_ACTION_ALLOW ? "ALLOWED ✓" :
                   result.action == NIMCP_POLICY_ACTION_DENY ? "DENIED" : "UNKNOWN");
            printf("   Evaluation time: %lu ns\n", result.eval_time_ns);
            nimcp_policy_result_free(&result);
        }

        nimcp_policy_context_destroy(ctx);
    }
    printf("\n");

    /* Test Case 3: Input Too Long */
    printf("Test Case 3: Input Length Validation\n");
    {
        nimcp_policy_context_t ctx = nimcp_policy_context_create();

        char* long_input = malloc(2000);
        memset(long_input, 'A', 1999);
        long_input[1999] = '\0';

        nimcp_policy_context_set_string(ctx, "input", long_input);
        nimcp_policy_context_set_string(ctx, "username", "charlie");

        nimcp_policy_result_t result = {0};
        err = nimcp_policy_evaluate(engine, ctx, &result);

        if (err == NIMCP_OK) {
            printf("   Input: %zu characters\n", strlen(long_input));
            printf("   Result: %s\n",
                   result.action == NIMCP_POLICY_ACTION_DENY ? "DENIED ✓" : "ALLOWED");
            printf("   Evaluation time: %lu ns\n", result.eval_time_ns);
            nimcp_policy_result_free(&result);
        }

        free(long_input);
        nimcp_policy_context_destroy(ctx);
    }
    printf("\n");

    /* Test Case 4: Custom Function - Unsafe User */
    printf("Test Case 4: Custom Function - Unsafe User\n");
    {
        nimcp_policy_context_t ctx = nimcp_policy_context_create();
        nimcp_policy_context_set_string(ctx, "input", "safe input");
        nimcp_policy_context_set_string(ctx, "username", "hacker123");

        nimcp_policy_result_t result = {0};
        err = nimcp_policy_evaluate(engine, ctx, &result);

        if (err == NIMCP_OK) {
            printf("   Username: \"hacker123\"\n");
            printf("   Result: %s\n",
                   result.action == NIMCP_POLICY_ACTION_DENY ? "DENIED ✓" : "ALLOWED");
            printf("   Evaluation time: %lu ns\n", result.eval_time_ns);
            nimcp_policy_result_free(&result);
        }

        nimcp_policy_context_destroy(ctx);
    }
    printf("\n");

    /* ================================================================
     * 6. Display Statistics
     * ================================================================ */
    printf("6. Policy Engine Statistics\n");

    nimcp_policy_stats_t stats = {0};
    err = nimcp_policy_engine_get_stats(engine, &stats);
    if (err == NIMCP_OK) {
        printf("   Total policies: %zu\n", stats.num_policies);
        printf("   Total evaluations: %lu\n", stats.total_evaluations);
        printf("   Average eval time: %lu ns\n", stats.avg_eval_time_ns);
        printf("   Max eval time: %lu ns\n", stats.max_eval_time_ns);
        printf("   Parse errors: %zu\n", stats.parse_errors);
        printf("   Eval errors: %zu\n", stats.eval_errors);
    }
    printf("\n");

    /* ================================================================
     * 7. Performance Test
     * ================================================================ */
    printf("7. Performance Test (1000 evaluations)\n");
    {
        nimcp_policy_context_t ctx = nimcp_policy_context_create();
        nimcp_policy_context_set_string(ctx, "input", "test input");
        nimcp_policy_context_set_string(ctx, "username", "testuser");

        struct timespec start, end;
        clock_gettime(CLOCK_MONOTONIC, &start);

        for (int i = 0; i < 1000; i++) {
            nimcp_policy_result_t result = {0};
            nimcp_policy_evaluate(engine, ctx, &result);
            nimcp_policy_result_free(&result);
        }

        clock_gettime(CLOCK_MONOTONIC, &end);

        uint64_t duration_ns = (end.tv_sec - start.tv_sec) * 1000000000ULL +
                              (end.tv_nsec - start.tv_nsec);
        double avg_us = (double)duration_ns / 1000.0 / 1000.0;

        printf("   Total time: %lu ns\n", duration_ns);
        printf("   Average per evaluation: %.2f µs\n", avg_us);

        nimcp_policy_context_destroy(ctx);
    }
    printf("\n");

    printf("=== Demo Complete ===\n");

cleanup:
    nimcp_policy_engine_destroy(engine);
    return 0;
}
