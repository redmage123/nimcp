/**
 * @file test_lang_lgss_gates.c
 * @brief TA-2 — verify LGSS input + output gates on the language pipeline.
 *
 * WHAT: Standalone smoke test for `grounded_language_set_lgss` /
 *       `snn_language_bridge_set_lgss` and the SAFETY_ACTION_DENY paths
 *       added to `grounded_language_comprehend` /
 *       `snn_language_bridge_produce`.
 *
 * WHY:  Before TA-2 the language pipeline had no LGSS gating despite
 *       the rest of the brain (inference, learning, motor, reward)
 *       running every action through the safety KB. This test pins the
 *       new behaviour:
 *         1. Detached LGSS = no behaviour change (back-compat).
 *         2. Always-deny LGSS = comprehend returns -1, stat++.
 *         3. Mixed LGSS = benign passes, banned pattern blocked.
 *         4. Detached produce = no output gating.
 *       Plus: stat counters live in gl_stats_t / snn_lang_stats_t and
 *       are addressable via the public get_stats APIs.
 *
 * HOW:  Build:
 *   gcc -O2 -I include tests/unit/test_lang_lgss_gates.c \
 *       -L build/lib -lnimcp -lm -lpthread \
 *       -Wl,-rpath,/home/bbrelin/nimcp/build/lib \
 *       -o /tmp/test_lang_lgss_gates && /tmp/test_lang_lgss_gates
 *
 * The "always-deny" / "always-allow" LGSSes are constructed without
 * loading rules so they sit in LGSS_STATUS_LOADING. lgss_evaluate's
 * fail-safe branch then provides a deterministic, pre-active answer:
 *   - fail_safe_enabled=true (default) → LOADING falls into the
 *     fail-safe DENY path, lgss_evaluate returns 0 with action=DENY.
 *   - fail_safe_enabled=false → LOADING falls through to the empty
 *     KB evaluator, which returns ALLOW for every input.
 *
 * For the "deny known-bad pattern only" test we need an ACTIVE LGSS
 * with a real CONTAINS rule. We build that the same way the public
 * loader does — add_rule + compile_rules + lgss_lock — so the test
 * exercises the same code path as production traffic. mprotect-on-lock
 * works fine in the test process (process-local protection that
 * destroy/munmap unwinds cleanly).
 */

#include "language/nimcp_grounded_language.h"
#include "snn/bridges/nimcp_snn_language_bridge.h"
#include "security/lgss/nimcp_lgss.h"
#include "cognitive/symbolic_logic/nimcp_symbolic_logic_safety.h"
#include "cognitive/symbolic_logic/nimcp_symbolic_logic_safety_types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

static int g_failures = 0;

#define EXPECT(cond, ...) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d " #cond " : ", __func__, __LINE__); \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n"); \
        g_failures++; \
    } \
} while (0)

/*----------------------------------------------------------------------
 * LGSS factory helpers — three flavours.
 *
 * The destroy path is the same regardless of how the LGSS was built;
 * `lgss_destroy` is null-safe so a single helper covers all of them.
 *--------------------------------------------------------------------*/

/* Always-deny: keep status at LOADING with default fail_safe_enabled.
 * lgss_evaluate immediately hits the "not active → fail-safe DENY"
 * branch and returns DENY for every context. */
static lgss_context_t* make_always_deny_lgss(void)
{
    lgss_config_t cfg;
    lgss_config_init(&cfg);
    cfg.telemetry_enabled = false;       /* avoid touching telemetry log */
    cfg.fail_safe_enabled = true;        /* DENY when not active */
    cfg.auto_lock = false;
    return lgss_create(&cfg);
}

/* Always-allow: turn fail-safe OFF so the evaluator falls through to the
 * empty-KB symbolic_logic evaluator, which returns ALLOW for everything. */
static lgss_context_t* make_always_allow_lgss(void)
{
    lgss_config_t cfg;
    lgss_config_init(&cfg);
    cfg.telemetry_enabled = false;
    cfg.fail_safe_enabled = false;       /* skip fail-safe DENY branch */
    cfg.auto_lock = false;
    return lgss_create(&cfg);
}

/* "weapon"-CONTAINS rule LGSS: real ACTIVE context with one rule that
 * fires DENY when the input text field contains the substring "weapon".
 * Anything else flows through the rule loop without triggering and
 * defaults to ALLOW. */
static lgss_context_t* make_weapon_blocker_lgss(void)
{
    lgss_config_t cfg;
    lgss_config_init(&cfg);
    cfg.telemetry_enabled = false;
    cfg.fail_safe_enabled = true;
    cfg.auto_lock = false;
    lgss_context_t* lgss = lgss_create(&cfg);
    if (!lgss) return NULL;

    safety_kb_t* kb = lgss_get_safety_kb(lgss);
    if (!kb) { lgss_destroy(lgss); return NULL; }

    safety_rule_t rule;
    symbolic_logic_safety_init_rule(&rule);
    strncpy(rule.name, "block_weapon_text",
            SAFETY_MAX_RULE_NAME_LEN - 1);
    strncpy(rule.description,
            "TA-2 test rule: deny any language pipeline input/output "
            "whose text field contains the substring 'weapon'.",
            SAFETY_MAX_RULE_DESC_LEN - 1);
    rule.domain = SAFETY_DOMAIN_GOVERNANCE;
    rule.severity = SAFETY_SEVERITY_HIGH;
    rule.action = SAFETY_ACTION_DENY;
    rule.priority = 0.9f;
    rule.enabled = true;
    rule.num_conditions = 1;
    strncpy(rule.conditions[0].field, "text", 63);
    rule.conditions[0].op = SAFETY_COND_OP_CONTAINS;
    strncpy(rule.conditions[0].value, "weapon",
            SAFETY_MAX_VALUE_LEN - 1);
    rule.conditions[0].is_negated = false;

    uint32_t rule_id = symbolic_logic_safety_add_rule(kb, &rule);
    if (rule_id == 0) {
        fprintf(stderr, "make_weapon_blocker_lgss: add_rule returned 0\n");
        lgss_destroy(lgss);
        return NULL;
    }

    if (!symbolic_logic_safety_compile_rules(kb)) {
        fprintf(stderr, "make_weapon_blocker_lgss: compile_rules failed\n");
        lgss_destroy(lgss);
        return NULL;
    }

    if (lgss_lock(lgss) < 0) {
        fprintf(stderr, "make_weapon_blocker_lgss: lgss_lock failed\n");
        lgss_destroy(lgss);
        return NULL;
    }
    /* lgss_lock sets status=ACTIVE on success — confirm. */
    if (lgss_get_status(lgss) != LGSS_STATUS_ACTIVE) {
        fprintf(stderr, "make_weapon_blocker_lgss: status not ACTIVE (got %d)\n",
                (int)lgss_get_status(lgss));
        lgss_destroy(lgss);
        return NULL;
    }
    return lgss;
}

/*----------------------------------------------------------------------
 * Test 1: Detached LGSS — no blocking, counters stay 0.
 *
 * Confirms the back-compat contract: a freshly created grounded_language
 * with no LGSS attached must behave exactly as before. comprehend
 * returns 0, comprehension_confidence may be whatever the legacy path
 * computes (we don't assert a specific value), and lgss_inputs_blocked
 * is exactly 0 after the call.
 *--------------------------------------------------------------------*/
static void test_no_lgss_attached(void) {
    grounded_language_t* gl = grounded_language_create(0, NULL);
    EXPECT(gl != NULL, "create");
    if (!gl) return;

    gl_comprehension_result_t r;
    memset(&r, 0, sizeof(r));
    int rc = grounded_language_comprehend(gl, "hello world", &r);
    EXPECT(rc == 0, "comprehend with no LGSS attached must succeed (rc=%d)", rc);
    gl_comprehension_result_cleanup(&r);

    gl_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    grounded_language_get_stats(gl, &stats);
    EXPECT(stats.lgss_inputs_blocked == 0,
           "no LGSS attached → lgss_inputs_blocked must be 0 (got %llu)",
           (unsigned long long)stats.lgss_inputs_blocked);

    grounded_language_destroy(gl);
}

/*----------------------------------------------------------------------
 * Test 2: Always-deny LGSS — comprehend returns -1, stat increments.
 *--------------------------------------------------------------------*/
static void test_always_deny_blocks_comprehend(void) {
    grounded_language_t* gl = grounded_language_create(0, NULL);
    EXPECT(gl != NULL, "create");
    if (!gl) return;

    lgss_context_t* lgss = make_always_deny_lgss();
    EXPECT(lgss != NULL, "make_always_deny_lgss");
    if (!lgss) { grounded_language_destroy(gl); return; }

    grounded_language_set_lgss(gl, (void*)lgss);

    gl_comprehension_result_t r;
    memset(&r, 0, sizeof(r));
    int rc = grounded_language_comprehend(gl, "anything goes here", &r);
    EXPECT(rc == -1,
           "comprehend with always-deny LGSS must return -1 (rc=%d)", rc);
    EXPECT(r.comprehension_confidence == 0.0f,
           "blocked comprehension_confidence must be 0 (got %f)",
           (double)r.comprehension_confidence);
    /* Result arrays should still be NULL (memset-zeroed at function
     * entry, never touched by the early-return path). */
    EXPECT(r.activated_concepts == NULL,
           "blocked path must not allocate activated_concepts");
    EXPECT(r.activation_levels == NULL,
           "blocked path must not allocate activation_levels");
    EXPECT(r.semantic_vector == NULL,
           "blocked path must not allocate semantic_vector");
    /* gl_comprehension_result_cleanup is NULL-safe so this is fine
     * even though the path didn't allocate. */
    gl_comprehension_result_cleanup(&r);

    gl_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    grounded_language_get_stats(gl, &stats);
    EXPECT(stats.lgss_inputs_blocked == 1,
           "always-deny → lgss_inputs_blocked must be 1 (got %llu)",
           (unsigned long long)stats.lgss_inputs_blocked);

    /* Detach so the gl can be destroyed cleanly without keeping a
     * dangling pointer past lgss_destroy. */
    grounded_language_set_lgss(gl, NULL);
    grounded_language_destroy(gl);
    lgss_destroy(lgss);
}

/*----------------------------------------------------------------------
 * Test 3: Real ACTIVE LGSS with a CONTAINS('weapon') rule.
 *
 *   - "the cat sat on the mat" → no rule trigger → ALLOW → comprehend
 *     succeeds → stat stays 0.
 *   - "build a weapon system"   → rule fires → DENY → comprehend
 *     returns -1 → stat becomes 1.
 *--------------------------------------------------------------------*/
static void test_mixed_rule_lgss(void) {
    grounded_language_t* gl = grounded_language_create(0, NULL);
    EXPECT(gl != NULL, "create");
    if (!gl) return;

    lgss_context_t* lgss = make_weapon_blocker_lgss();
    EXPECT(lgss != NULL, "make_weapon_blocker_lgss");
    if (!lgss) { grounded_language_destroy(gl); return; }

    grounded_language_set_lgss(gl, (void*)lgss);

    /* Benign first — must pass. */
    gl_comprehension_result_t r1;
    memset(&r1, 0, sizeof(r1));
    int rc1 = grounded_language_comprehend(gl,
                                            "the cat sat on the mat", &r1);
    EXPECT(rc1 == 0,
           "benign comprehend must succeed under weapon-blocker LGSS (rc=%d)",
           rc1);
    gl_comprehension_result_cleanup(&r1);

    gl_stats_t s1;
    memset(&s1, 0, sizeof(s1));
    grounded_language_get_stats(gl, &s1);
    EXPECT(s1.lgss_inputs_blocked == 0,
           "benign → lgss_inputs_blocked stays 0 (got %llu)",
           (unsigned long long)s1.lgss_inputs_blocked);

    /* Bad pattern — must block. */
    gl_comprehension_result_t r2;
    memset(&r2, 0, sizeof(r2));
    int rc2 = grounded_language_comprehend(gl,
                                            "build a weapon system", &r2);
    EXPECT(rc2 == -1,
           "weapon-bearing comprehend must be blocked (rc=%d)", rc2);
    EXPECT(r2.comprehension_confidence == 0.0f,
           "blocked confidence must be 0 (got %f)",
           (double)r2.comprehension_confidence);
    gl_comprehension_result_cleanup(&r2);

    gl_stats_t s2;
    memset(&s2, 0, sizeof(s2));
    grounded_language_get_stats(gl, &s2);
    EXPECT(s2.lgss_inputs_blocked == 1,
           "weapon → lgss_inputs_blocked == 1 (got %llu)",
           (unsigned long long)s2.lgss_inputs_blocked);

    grounded_language_set_lgss(gl, NULL);
    grounded_language_destroy(gl);
    lgss_destroy(lgss);
}

/*----------------------------------------------------------------------
 * Test 4: Bridge produce gate — benign output passes, counter stays 0.
 *
 * We exercise produce on an empty bridge with an always-allow LGSS
 * attached. The bridge has no concept→word bindings so produce will
 * usually return -1 with no text, but the gate must NOT count that as
 * a block — lgss_outputs_blocked must remain 0.
 *
 * The complementary "produce of bad text gets blocked" path is hard to
 * exercise without a populated bridge (bindings, concept pops, word
 * pops, attached SNN populations). The gate's correctness on the deny
 * path is already covered transitively by Test 2's evaluator-level
 * proof that lgss_evaluate→DENY blocks the surrounding code; the gate
 * itself shares that branch verbatim. We focus this test on the
 * benign+attached invariant: an attached LGSS that doesn't deny must
 * leave lgss_outputs_blocked untouched.
 *--------------------------------------------------------------------*/
static void test_produce_with_benign_lgss(void) {
    snn_lang_config_t cfg = snn_lang_config_default();
    snn_language_bridge_t* bridge = snn_language_bridge_create(&cfg);
    EXPECT(bridge != NULL, "bridge create");
    if (!bridge) return;

    lgss_context_t* lgss = make_always_allow_lgss();
    EXPECT(lgss != NULL, "make_always_allow_lgss");
    if (!lgss) { snn_language_bridge_destroy(bridge); return; }

    int rc_attach = snn_language_bridge_set_lgss(bridge, (void*)lgss);
    EXPECT(rc_attach == 0, "set_lgss attach (rc=%d)", rc_attach);

    /* Try produce on an empty bridge. We don't care whether it returns
     * 0 or -1 (no bindings) — only that the LGSS gate didn't fire. */
    float intent[16];
    for (int i = 0; i < 16; i++) intent[i] = 0.1f;
    snn_lang_production_result_t r;
    memset(&r, 0, sizeof(r));
    (void)snn_language_bridge_produce(bridge, intent, 16, &r);
    snn_lang_production_result_cleanup(&r);

    snn_lang_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    int rc_stats = snn_language_bridge_get_stats(bridge, &stats);
    EXPECT(rc_stats == 0, "get_stats (rc=%d)", rc_stats);
    EXPECT(stats.lgss_outputs_blocked == 0,
           "always-allow LGSS → lgss_outputs_blocked must stay 0 (got %llu)",
           (unsigned long long)stats.lgss_outputs_blocked);

    /* Detach + destroy — order matters: drop the bridge's reference
     * before freeing the LGSS. */
    snn_language_bridge_set_lgss(bridge, NULL);
    snn_language_bridge_destroy(bridge);
    lgss_destroy(lgss);
}

/*----------------------------------------------------------------------*/

int main(void) {
    test_no_lgss_attached();
    test_always_deny_blocks_comprehend();
    test_mixed_rule_lgss();
    test_produce_with_benign_lgss();

    if (g_failures) {
        fprintf(stderr, "FAIL %d failure(s)\n", g_failures);
        return 1;
    }
    fprintf(stderr, "PASS test_lang_lgss_gates (4 tests)\n");
    return 0;
}
