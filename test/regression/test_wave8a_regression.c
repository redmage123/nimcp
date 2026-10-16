/**
 * @file test_wave8a_regression.c
 * @brief Regression tests for Wave 8A enum/coordinator extensions.
 *
 * WHAT: Guards that adding BRAIN_CYCLE_COCHLEA_BRIDGES to brain_cycle_type_t
 *       did not shift any existing enum value and that all 16 cycle types
 *       (15 pre-Wave-8A + 1 Wave 8A) still map to the name, category, and
 *       default interval they had before.
 *
 * WHY:  Enum extensions are a common ABI/behavior-regression source: a
 *       typo reorders the switch, a missing case falls through to
 *       BACKGROUND, downstream callers querying get_default_interval_us
 *       silently get 0. Wave 8A also grew the enum count from 15 → 16 so
 *       any downstream code sizing a buffer to [BRAIN_CYCLE_COUNT] is
 *       exercised under the new dimension.
 *
 * HOW:  Hard-coded golden expectations per cycle — this is the contract.
 *       If someone reorders the enum this test fails loudly.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "core/brain/nimcp_brain_cycle_coordinator.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { printf("  %-72s", name); fflush(stdout); } while(0)
#define PASS() do { printf("[PASS]\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("[FAIL] %s\n", msg); tests_failed++; return; } while(0)
#define ASSERT_TRUE(cond, msg) do { if (!(cond)) { FAIL(msg); } } while(0)
#define ASSERT_STREQ(a, b, msg) do { if (strcmp((a), (b)) != 0) { \
    printf("[FAIL] %s (got '%s', expected '%s')\n", msg, a, b); \
    tests_failed++; return; } } while(0)
#define ASSERT_EQ(a, b, msg) do { if ((long long)(a) != (long long)(b)) { \
    printf("[FAIL] %s (got %lld, expected %lld)\n", msg, (long long)(a), (long long)(b)); \
    tests_failed++; return; } } while(0)
#define ASSERT_NOT_NULL(p, msg) do { if ((p) == NULL) { FAIL(msg); } } while(0)

static void make_quiet_cfg(brain_cycle_coordinator_config_t* c) {
    brain_cycle_coordinator_default_config(c);
    c->enable_logging = false;
    c->enable_debug_logging = false;
}

static void nosleep_tick_fn(void* ctx) { (void)ctx; }

static void sleep_ms(int ms) {
    struct timespec ts = {ms / 1000, (long)(ms % 1000) * 1000000L};
    nanosleep(&ts, NULL);
}

/* ------------------------------------------------------------------------- */
/* Per-cycle "golden" expectations                                           */
/* ------------------------------------------------------------------------- */

typedef struct {
    brain_cycle_type_t     type;
    const char*            name;
    brain_cycle_category_t category;
    uint64_t               default_interval_us;
} cycle_golden_t;

/* Order matches enum declaration in nimcp_brain_cycle_coordinator.h. */
static const cycle_golden_t GOLDEN[] = {
    /* Pre-Wave-8A cycles — these must not drift. */
    { BRAIN_CYCLE_IMMUNE_TICK,      "immune_tick",      BRAIN_CYCLE_CATEGORY_FAST,       50000    },
    { BRAIN_CYCLE_HEALTH_AGENT,     "health_agent",     BRAIN_CYCLE_CATEGORY_MEDIUM,     100000   },
    { BRAIN_CYCLE_SLEEP_WAKE,       "sleep_wake",       BRAIN_CYCLE_CATEGORY_SLOW,       0        },
    { BRAIN_CYCLE_CIRCADIAN,        "circadian",        BRAIN_CYCLE_CATEGORY_SLOW,       0        },
    { BRAIN_CYCLE_AROUSAL,          "arousal",          BRAIN_CYCLE_CATEGORY_SLOW,       0        },
    { BRAIN_CYCLE_OSCILLATIONS,     "oscillations",     BRAIN_CYCLE_CATEGORY_FAST,       10000    },
    { BRAIN_CYCLE_GC_AGENT,         "gc_agent",         BRAIN_CYCLE_CATEGORY_BACKGROUND, 60000000 },
    { BRAIN_CYCLE_IO_DISPATCHER,    "io_dispatcher",    BRAIN_CYCLE_CATEGORY_BACKGROUND, 0        },
    { BRAIN_CYCLE_BRAIN_UPDATE,     "brain_update",     BRAIN_CYCLE_CATEGORY_FAST,       16000    },
    { BRAIN_CYCLE_LONG_TERM_MEMORY, "long_term_memory", BRAIN_CYCLE_CATEGORY_MEDIUM,     100000   },
    { BRAIN_CYCLE_NEUROGENESIS,     "neurogenesis",     BRAIN_CYCLE_CATEGORY_SLOW,       1000000  },
    { BRAIN_CYCLE_EPIGENETICS,      "epigenetics",      BRAIN_CYCLE_CATEGORY_MEDIUM,     100000   },
    { BRAIN_CYCLE_NEUROVASCULAR,    "neurovascular",    BRAIN_CYCLE_CATEGORY_MEDIUM,     100000   },
    { BRAIN_CYCLE_PREDICTIVE_IMMUNE,"predictive_immune",BRAIN_CYCLE_CATEGORY_MEDIUM,     100000   },
    { BRAIN_CYCLE_CHEMISTRY,        "chemistry",        BRAIN_CYCLE_CATEGORY_FAST,       10000    },
    /* NEW in Wave 8A — explicit expectation so it too is pinned. */
    { BRAIN_CYCLE_COCHLEA_BRIDGES,  "cochlea_bridges",  BRAIN_CYCLE_CATEGORY_FAST,       10000    },
};

static void test_enum_contract_per_cycle(void) {
    TEST("all 16 cycle types: name + category + default_interval pinned");
    const size_t n = sizeof(GOLDEN) / sizeof(GOLDEN[0]);
    for (size_t i = 0; i < n; i++) {
        const cycle_golden_t* g = &GOLDEN[i];
        const char* nm = brain_cycle_type_name(g->type);
        ASSERT_NOT_NULL(nm, "type name returned NULL");
        ASSERT_STREQ(nm, g->name, "cycle name drift");

        brain_cycle_category_t cat = brain_cycle_get_category(g->type);
        ASSERT_EQ((int)cat, (int)g->category, "category drift");

        uint64_t iv = brain_cycle_get_default_interval_us(g->type);
        ASSERT_EQ(iv, g->default_interval_us, "default_interval drift");
    }
    PASS();
}

static void test_enum_count_floor_is_16(void) {
    /* Wave 8A added BRAIN_CYCLE_COCHLEA_BRIDGES; count grew 15 → 16.
     * Floor guard: new waves may append above, but nothing should shrink
     * the enum below Wave 8A's floor. */
    TEST("BRAIN_CYCLE_COUNT >= 16 (Wave 8A floor — new cycles allowed above)");
    ASSERT_TRUE((int)BRAIN_CYCLE_COUNT >= 16,
                "BRAIN_CYCLE_COUNT dropped below Wave 8A floor");
    PASS();
}

static void test_enum_cochlea_bridges_slot(void) {
    TEST("BRAIN_CYCLE_COCHLEA_BRIDGES ordinal is 15 (Wave 8A position)");
    ASSERT_EQ((int)BRAIN_CYCLE_COCHLEA_BRIDGES, 15,
              "COCHLEA_BRIDGES ordinal shifted — enum reordering is a "
              "breaking change for tests and persisted state");
    PASS();
}

static void test_enum_chemistry_slot_unchanged(void) {
    /* CHEMISTRY should stay at ordinal 14 regardless of what waves append. */
    TEST("BRAIN_CYCLE_CHEMISTRY ordinal is 14 (Wave 6 position, unchanged)");
    ASSERT_EQ((int)BRAIN_CYCLE_CHEMISTRY, 14,
              "CHEMISTRY slot drifted — enum reordered");
    PASS();
}

static void test_enum_predictive_immune_slot_unchanged(void) {
    /* PREDICTIVE_IMMUNE should stay at ordinal 13 regardless of new waves. */
    TEST("BRAIN_CYCLE_PREDICTIVE_IMMUNE ordinal is 13 (Wave 4 position)");
    ASSERT_EQ((int)BRAIN_CYCLE_PREDICTIVE_IMMUNE, 13,
              "PREDICTIVE_IMMUNE slot drifted — enum reordered");
    PASS();
}

static void test_enum_early_cycles_unchanged(void) {
    TEST("early cycles (0..12) ordinals pinned");
    ASSERT_EQ((int)BRAIN_CYCLE_IMMUNE_TICK,      0,  "IMMUNE_TICK slot drifted");
    ASSERT_EQ((int)BRAIN_CYCLE_HEALTH_AGENT,     1,  "HEALTH_AGENT slot drifted");
    ASSERT_EQ((int)BRAIN_CYCLE_SLEEP_WAKE,       2,  "SLEEP_WAKE slot drifted");
    ASSERT_EQ((int)BRAIN_CYCLE_CIRCADIAN,        3,  "CIRCADIAN slot drifted");
    ASSERT_EQ((int)BRAIN_CYCLE_AROUSAL,          4,  "AROUSAL slot drifted");
    ASSERT_EQ((int)BRAIN_CYCLE_OSCILLATIONS,     5,  "OSCILLATIONS slot drifted");
    ASSERT_EQ((int)BRAIN_CYCLE_GC_AGENT,         6,  "GC_AGENT slot drifted");
    ASSERT_EQ((int)BRAIN_CYCLE_IO_DISPATCHER,    7,  "IO_DISPATCHER slot drifted");
    ASSERT_EQ((int)BRAIN_CYCLE_BRAIN_UPDATE,     8,  "BRAIN_UPDATE slot drifted");
    ASSERT_EQ((int)BRAIN_CYCLE_LONG_TERM_MEMORY, 9,  "LONG_TERM_MEMORY slot drifted");
    ASSERT_EQ((int)BRAIN_CYCLE_NEUROGENESIS,     10, "NEUROGENESIS slot drifted");
    ASSERT_EQ((int)BRAIN_CYCLE_EPIGENETICS,      11, "EPIGENETICS slot drifted");
    ASSERT_EQ((int)BRAIN_CYCLE_NEUROVASCULAR,    12, "NEUROVASCULAR slot drifted");
    PASS();
}

/* ------------------------------------------------------------------------- */
/* Behavior of register_driven on the new type mirrors existing types        */
/* ------------------------------------------------------------------------- */

static void test_register_driven_on_cochlea_bridges_type(void) {
    TEST("register_driven(COCHLEA_BRIDGES) works + unregister joins");
    brain_cycle_coordinator_config_t cfg; make_quiet_cfg(&cfg);
    brain_cycle_coordinator_t* coord = brain_cycle_coordinator_create(&cfg);
    ASSERT_NOT_NULL(coord, "create failed");

    int rc = brain_cycle_coordinator_register_driven(
        coord, BRAIN_CYCLE_COCHLEA_BRIDGES, 5000,
        nosleep_tick_fn, NULL, NULL);
    ASSERT_EQ(rc, 0, "register_driven(COCHLEA_BRIDGES) failed");

    sleep_ms(50);

    brain_cycle_status_t st;
    int g = brain_cycle_coordinator_get_status(
        coord, BRAIN_CYCLE_COCHLEA_BRIDGES, &st);
    ASSERT_EQ(g, 0, "get_status failed");
    ASSERT_TRUE(st.ticks_executed >= 1,
        "tick count zero — driver thread never ran");

    int ur = brain_cycle_coordinator_unregister(
        coord, BRAIN_CYCLE_COCHLEA_BRIDGES);
    ASSERT_EQ(ur, 0, "unregister failed");

    /* Second unregister fails (already gone). */
    int ur2 = brain_cycle_coordinator_unregister(
        coord, BRAIN_CYCLE_COCHLEA_BRIDGES);
    ASSERT_EQ(ur2, -1, "second unregister should fail");

    brain_cycle_coordinator_destroy(coord);
    PASS();
}

/* ------------------------------------------------------------------------- */
/* get_all_status returns the right shape with the new enum value            */
/* ------------------------------------------------------------------------- */

static void test_get_all_status_buffer_sized_to_count(void) {
    TEST("get_all_status honors BRAIN_CYCLE_COUNT buffer size at N>=16");
    brain_cycle_coordinator_config_t cfg; make_quiet_cfg(&cfg);
    brain_cycle_coordinator_t* coord = brain_cycle_coordinator_create(&cfg);
    ASSERT_NOT_NULL(coord, "create failed");

    int r1 = brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_BRAIN_UPDATE, NULL, NULL);
    int r2 = brain_cycle_coordinator_register_driven(
        coord, BRAIN_CYCLE_CHEMISTRY, 10000, nosleep_tick_fn, NULL, NULL);
    int r3 = brain_cycle_coordinator_register_driven(
        coord, BRAIN_CYCLE_COCHLEA_BRIDGES, 10000, nosleep_tick_fn, NULL, NULL);
    ASSERT_EQ(r1, 0, "register BRAIN_UPDATE failed");
    ASSERT_EQ(r2, 0, "register_driven CHEMISTRY failed");
    ASSERT_EQ(r3, 0, "register_driven COCHLEA_BRIDGES failed");

    /* Buffer sized to BRAIN_CYCLE_COUNT entries per the public contract. */
    brain_cycle_status_t statuses[BRAIN_CYCLE_COUNT];
    uint32_t count = 0;
    int rc = brain_cycle_coordinator_get_all_status(
        coord, statuses, &count);

    (void)brain_cycle_coordinator_unregister(coord, BRAIN_CYCLE_BRAIN_UPDATE);
    (void)brain_cycle_coordinator_unregister(coord, BRAIN_CYCLE_CHEMISTRY);
    (void)brain_cycle_coordinator_unregister(coord, BRAIN_CYCLE_COCHLEA_BRIDGES);
    brain_cycle_coordinator_destroy(coord);

    ASSERT_EQ(rc, 0, "get_all_status failed");
    ASSERT_TRUE(count >= 3, "get_all_status returned fewer than 3 cycles");
    ASSERT_TRUE(count <= BRAIN_CYCLE_COUNT,
        "get_all_status returned MORE than BRAIN_CYCLE_COUNT");
    PASS();
}

/* ------------------------------------------------------------------------- */

int main(void) {
    printf("\n=== Regression Tests: Wave 8A enum + coordinator contracts ===\n\n");

    test_enum_count_floor_is_16();
    test_enum_cochlea_bridges_slot();
    test_enum_chemistry_slot_unchanged();
    test_enum_predictive_immune_slot_unchanged();
    test_enum_early_cycles_unchanged();
    test_enum_contract_per_cycle();
    test_register_driven_on_cochlea_bridges_type();
    test_get_all_status_buffer_sized_to_count();

    printf("\n=== Results: %d passed, %d failed ===\n\n",
           tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
