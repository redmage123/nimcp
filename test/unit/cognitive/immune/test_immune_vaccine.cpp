/**
 * @file test_immune_vaccine.cpp
 * @brief Unit tests for Immune Vaccine Module
 * @date 2025-12-12
 *
 * Tests vaccine entry creation, administration, passive immunity,
 * booster scheduling, and efficacy tracking.
 */

#include <gtest/gtest.h>
#include <cstring>

// Headers have their own extern "C" guards
#include "cognitive/immune/nimcp_immune_vaccine.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class ImmuneVaccineTest : public ::testing::Test {
protected:
    brain_immune_system_t* immune_system = nullptr;
    vaccine_system_t* vaccine = nullptr;
    vaccine_config_t config;

    void SetUp() override {
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune_system = brain_immune_create(&immune_config);
        ASSERT_NE(immune_system, nullptr);
        brain_immune_start(immune_system);

        vaccine_default_config(&config);
        vaccine = vaccine_create(&config, immune_system);
        ASSERT_NE(vaccine, nullptr);
        vaccine_start(vaccine);
    }

    void TearDown() override {
        if (vaccine) {
            vaccine_stop(vaccine);
            vaccine_destroy(vaccine);
            vaccine = nullptr;
        }
        if (immune_system) {
            brain_immune_stop(immune_system);
            brain_immune_destroy(immune_system);
            immune_system = nullptr;
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(ImmuneVaccineTest, DefaultConfigIsValid) {
    vaccine_config_t cfg;
    int result = vaccine_default_config(&cfg);
    EXPECT_EQ(result, 0);
    EXPECT_GT(cfg.max_vaccines, 0u);
    EXPECT_GT(cfg.default_affinity, 0.0f);
}

TEST_F(ImmuneVaccineTest, DefaultConfigNullFails) {
    int result = vaccine_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(ImmuneVaccineTest, CreateWithNullConfig) {
    vaccine_system_t* sys = vaccine_create(nullptr, immune_system);
    ASSERT_NE(sys, nullptr);
    vaccine_stop(sys);
    vaccine_destroy(sys);
}

TEST_F(ImmuneVaccineTest, DestroyNullSafe) {
    vaccine_destroy(nullptr);
}

TEST_F(ImmuneVaccineTest, StartStopCycle) {
    vaccine_system_t* sys = vaccine_create(&config, immune_system);
    ASSERT_NE(sys, nullptr);

    EXPECT_EQ(vaccine_start(sys), 0);
    EXPECT_TRUE(sys->running);

    EXPECT_EQ(vaccine_stop(sys), 0);
    EXPECT_FALSE(sys->running);

    vaccine_destroy(sys);
}

/* ============================================================================
 * Vaccine Entry Creation Tests
 * ============================================================================ */

TEST_F(ImmuneVaccineTest, CreateVaccineEntry) {
    uint8_t epitope[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    uint32_t vaccine_id;

    int result = vaccine_create_entry(vaccine, VACCINE_TYPE_INACTIVATED,
                                      epitope, sizeof(epitope), "Test Vaccine", &vaccine_id);
    EXPECT_EQ(result, 0);
    EXPECT_GT(vaccine_id, 0u);
}

TEST_F(ImmuneVaccineTest, CreateMultipleVaccineTypes) {
    uint8_t epitope[] = {0x01, 0x02, 0x03};
    uint32_t id1, id2, id3, id4, id5;

    vaccine_create_entry(vaccine, VACCINE_TYPE_ATTENUATED, epitope, sizeof(epitope), "Att", &id1);
    vaccine_create_entry(vaccine, VACCINE_TYPE_INACTIVATED, epitope, sizeof(epitope), "Inact", &id2);
    vaccine_create_entry(vaccine, VACCINE_TYPE_SUBUNIT, epitope, sizeof(epitope), "Sub", &id3);
    vaccine_create_entry(vaccine, VACCINE_TYPE_MRNA, epitope, sizeof(epitope), "mRNA", &id4);
    vaccine_create_entry(vaccine, VACCINE_TYPE_PASSIVE, epitope, sizeof(epitope), "Pass", &id5);

    // All should have unique IDs
    EXPECT_NE(id1, id2);
    EXPECT_NE(id2, id3);
    EXPECT_NE(id3, id4);
    EXPECT_NE(id4, id5);
}

TEST_F(ImmuneVaccineTest, SetVaccineProperties) {
    uint8_t epitope[] = {0x01, 0x02, 0x03, 0x04};
    uint32_t vaccine_id;
    vaccine_create_entry(vaccine, VACCINE_TYPE_INACTIVATED, epitope, sizeof(epitope),
                         "Test", &vaccine_id);

    int result = vaccine_set_properties(vaccine, vaccine_id, 0.3f, 0.9f, 0.01f);
    EXPECT_EQ(result, 0);

    const vaccine_entry_t* entry = vaccine_get_entry(vaccine, vaccine_id);
    ASSERT_NE(entry, nullptr);
    EXPECT_FLOAT_EQ(entry->attenuation_factor, 0.3f);
    EXPECT_FLOAT_EQ(entry->initial_affinity, 0.9f);
}

TEST_F(ImmuneVaccineTest, SetVaccineDescription) {
    uint8_t epitope[] = {0x01, 0x02, 0x03, 0x04};
    uint32_t vaccine_id;
    vaccine_create_entry(vaccine, VACCINE_TYPE_INACTIVATED, epitope, sizeof(epitope),
                         "Test", &vaccine_id);

    int result = vaccine_set_description(vaccine, vaccine_id, "Protection against XYZ threat");
    EXPECT_EQ(result, 0);

    const vaccine_entry_t* entry = vaccine_get_entry(vaccine, vaccine_id);
    ASSERT_NE(entry, nullptr);
    EXPECT_STRNE(entry->description, "");
}

/* ============================================================================
 * Vaccine Administration Tests
 * ============================================================================ */

TEST_F(ImmuneVaccineTest, AdministerVaccine) {
    uint8_t epitope[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    uint32_t vaccine_id;
    vaccine_create_entry(vaccine, VACCINE_TYPE_INACTIVATED, epitope, sizeof(epitope),
                         "Test", &vaccine_id);

    // Note: Administration succeeds using internal antigen creation
    int result = vaccine_administer(vaccine, vaccine_id);
    EXPECT_EQ(result, 0);

    const vaccine_entry_t* entry = vaccine_get_entry(vaccine, vaccine_id);
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->status, VACCINE_STATUS_ADMINISTERED);
}

TEST_F(ImmuneVaccineTest, AdministerAttenuatedVaccine) {
    uint8_t epitope[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    uint32_t vaccine_id;
    vaccine_create_entry(vaccine, VACCINE_TYPE_ATTENUATED, epitope, sizeof(epitope),
                         "Live Attenuated", &vaccine_id);

    int result = vaccine_administer_attenuated(vaccine, vaccine_id, 0.9f);
    EXPECT_EQ(result, 0);
}

TEST_F(ImmuneVaccineTest, AdministerNonExistentFails) {
    int result = vaccine_administer(vaccine, 99999);
    EXPECT_NE(result, 0);
}

TEST_F(ImmuneVaccineTest, AdministerCreatesMemoryBCell) {
    uint8_t epitope[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    uint32_t vaccine_id;
    vaccine_create_entry(vaccine, VACCINE_TYPE_INACTIVATED, epitope, sizeof(epitope),
                         "Test", &vaccine_id);

    // Administration succeeds
    int result = vaccine_administer(vaccine, vaccine_id);
    EXPECT_EQ(result, 0);

    // Memory B cell should be created
    const vaccine_entry_t* entry = vaccine_get_entry(vaccine, vaccine_id);
    ASSERT_NE(entry, nullptr);
    EXPECT_GT(entry->memory_b_cell_id, 0u);
}

/* ============================================================================
 * Booster Tests
 * ============================================================================ */

TEST_F(ImmuneVaccineTest, AdministerBooster) {
    uint8_t epitope[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    uint32_t vaccine_id;
    vaccine_create_entry(vaccine, VACCINE_TYPE_INACTIVATED, epitope, sizeof(epitope),
                         "Test", &vaccine_id);
    vaccine_administer(vaccine, vaccine_id);

    int result = vaccine_booster(vaccine, vaccine_id);
    EXPECT_EQ(result, 0);

    const vaccine_entry_t* entry = vaccine_get_entry(vaccine, vaccine_id);
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->booster_count, 1u);
    EXPECT_EQ(entry->status, VACCINE_STATUS_BOOSTED);
}

TEST_F(ImmuneVaccineTest, BoosterIncreasesEfficacy) {
    uint8_t epitope[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    uint32_t vaccine_id;
    vaccine_create_entry(vaccine, VACCINE_TYPE_INACTIVATED, epitope, sizeof(epitope),
                         "Test", &vaccine_id);
    vaccine_administer(vaccine, vaccine_id);

    // Apply some decay
    vaccine_update_efficacy(vaccine, vaccine_id, 86400000);

    float before_booster;
    vaccine_get_efficacy(vaccine, vaccine_id, &before_booster);

    vaccine_booster(vaccine, vaccine_id);

    float after_booster;
    vaccine_get_efficacy(vaccine, vaccine_id, &after_booster);

    EXPECT_GE(after_booster, before_booster);
}

TEST_F(ImmuneVaccineTest, BoosterNonAdministeredSucceeds) {
    uint8_t epitope[] = {0x01, 0x02, 0x03, 0x04};
    uint32_t vaccine_id;
    vaccine_create_entry(vaccine, VACCINE_TYPE_INACTIVATED, epitope, sizeof(epitope),
                         "Test", &vaccine_id);

    // Note: The booster implementation doesn't require prior administration
    // It will boost any existing vaccine entry
    int result = vaccine_booster(vaccine, vaccine_id);
    EXPECT_EQ(result, 0);

    // Status should be BOOSTED after successful booster
    const vaccine_entry_t* entry = vaccine_get_entry(vaccine, vaccine_id);
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->status, VACCINE_STATUS_BOOSTED);
}

/* ============================================================================
 * Passive Immunity Tests
 * ============================================================================ */

TEST_F(ImmuneVaccineTest, ImportPassiveImmunity) {
    uint8_t epitope[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    uint32_t vaccine_id;

    // Import passive immunity succeeds
    int result = vaccine_import_passive_immunity(vaccine, epitope, sizeof(epitope),
                                                 0.8f, "External source", &vaccine_id);
    EXPECT_EQ(result, 0);

    // Verify vaccine was created
    const vaccine_entry_t* entry = vaccine_get_entry(vaccine, vaccine_id);
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->type, VACCINE_TYPE_PASSIVE);
}

/* ============================================================================
 * Scheduling Tests
 * ============================================================================ */

TEST_F(ImmuneVaccineTest, ScheduleVaccine) {
    uint8_t epitope[] = {0x01, 0x02, 0x03, 0x04};
    uint32_t vaccine_id;
    vaccine_create_entry(vaccine, VACCINE_TYPE_INACTIVATED, epitope, sizeof(epitope),
                         "Test", &vaccine_id);

    uint64_t future_time = 86400000;  // 1 day from now
    int result = vaccine_schedule_add(vaccine, vaccine_id, future_time);
    EXPECT_EQ(result, 0);
}

TEST_F(ImmuneVaccineTest, ScheduleBooster) {
    uint8_t epitope[] = {0x01, 0x02, 0x03, 0x04};
    uint32_t vaccine_id;
    vaccine_create_entry(vaccine, VACCINE_TYPE_INACTIVATED, epitope, sizeof(epitope),
                         "Test", &vaccine_id);
    vaccine_administer(vaccine, vaccine_id);

    int result = vaccine_schedule_booster(vaccine, vaccine_id, VACCINE_BOOSTER_STANDARD);
    EXPECT_EQ(result, 0);
}

TEST_F(ImmuneVaccineTest, CancelScheduledVaccine) {
    uint8_t epitope[] = {0x01, 0x02, 0x03, 0x04};
    uint32_t vaccine_id;
    vaccine_create_entry(vaccine, VACCINE_TYPE_INACTIVATED, epitope, sizeof(epitope),
                         "Test", &vaccine_id);

    vaccine_schedule_add(vaccine, vaccine_id, 86400000);

    int result = vaccine_schedule_cancel(vaccine, vaccine_id);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Efficacy Tracking Tests
 * ============================================================================ */

TEST_F(ImmuneVaccineTest, GetEfficacy) {
    uint8_t epitope[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    uint32_t vaccine_id;
    vaccine_create_entry(vaccine, VACCINE_TYPE_INACTIVATED, epitope, sizeof(epitope),
                         "Test", &vaccine_id);
    vaccine_administer(vaccine, vaccine_id);

    float efficacy;
    int result = vaccine_get_efficacy(vaccine, vaccine_id, &efficacy);
    EXPECT_EQ(result, 0);
    EXPECT_GT(efficacy, 0.0f);
    EXPECT_LE(efficacy, 1.0f);
}

TEST_F(ImmuneVaccineTest, EfficacyDecaysOverTime) {
    uint8_t epitope[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    uint32_t vaccine_id;
    vaccine_create_entry(vaccine, VACCINE_TYPE_INACTIVATED, epitope, sizeof(epitope),
                         "Test", &vaccine_id);
    vaccine_administer(vaccine, vaccine_id);

    float initial;
    vaccine_get_efficacy(vaccine, vaccine_id, &initial);

    // Simulate significant time passing
    vaccine_update_efficacy(vaccine, vaccine_id, 365L * 24 * 3600 * 1000);  // 1 year

    float after;
    vaccine_get_efficacy(vaccine, vaccine_id, &after);

    EXPECT_LT(after, initial);
}

TEST_F(ImmuneVaccineTest, RecordSuccess) {
    uint8_t epitope[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    uint32_t vaccine_id;
    vaccine_create_entry(vaccine, VACCINE_TYPE_INACTIVATED, epitope, sizeof(epitope),
                         "Test", &vaccine_id);
    vaccine_administer(vaccine, vaccine_id);

    int result = vaccine_record_success(vaccine, vaccine_id);
    EXPECT_EQ(result, 0);

    const vaccine_entry_t* entry = vaccine_get_entry(vaccine, vaccine_id);
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->exposures_prevented, 1u);
}

TEST_F(ImmuneVaccineTest, RecordFailure) {
    uint8_t epitope[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    uint32_t vaccine_id;
    vaccine_create_entry(vaccine, VACCINE_TYPE_INACTIVATED, epitope, sizeof(epitope),
                         "Test", &vaccine_id);
    vaccine_administer(vaccine, vaccine_id);

    int result = vaccine_record_failure(vaccine, vaccine_id);
    EXPECT_EQ(result, 0);

    const vaccine_entry_t* entry = vaccine_get_entry(vaccine, vaccine_id);
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->exposures_failed, 1u);
}

/* ============================================================================
 * Query API Tests
 * ============================================================================ */

TEST_F(ImmuneVaccineTest, GetEntryById) {
    uint8_t epitope[] = {0x01, 0x02, 0x03, 0x04};
    uint32_t vaccine_id;
    vaccine_create_entry(vaccine, VACCINE_TYPE_INACTIVATED, epitope, sizeof(epitope),
                         "Test", &vaccine_id);

    const vaccine_entry_t* entry = vaccine_get_entry(vaccine, vaccine_id);
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->id, vaccine_id);
}

TEST_F(ImmuneVaccineTest, GetEntryNonExistent) {
    const vaccine_entry_t* entry = vaccine_get_entry(vaccine, 99999);
    EXPECT_EQ(entry, nullptr);
}

TEST_F(ImmuneVaccineTest, FindByEpitope) {
    uint8_t epitope[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    uint32_t created_id;
    vaccine_create_entry(vaccine, VACCINE_TYPE_INACTIVATED, epitope, sizeof(epitope),
                         "Test", &created_id);

    uint32_t found_id;
    int result = vaccine_find_by_epitope(vaccine, epitope, sizeof(epitope), &found_id);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(found_id, created_id);
}

TEST_F(ImmuneVaccineTest, FindByEpitopeNotFound) {
    uint8_t epitope[] = {0xFF, 0xFE, 0xFD, 0xFC, 0xFB};
    uint32_t found_id;
    int result = vaccine_find_by_epitope(vaccine, epitope, sizeof(epitope), &found_id);
    EXPECT_EQ(result, -1);
}

TEST_F(ImmuneVaccineTest, GetActiveVaccines) {
    uint8_t epitope1[] = {0x01, 0x02, 0x03, 0x04};
    uint8_t epitope2[] = {0x05, 0x06, 0x07, 0x08};
    uint32_t id1, id2;

    vaccine_create_entry(vaccine, VACCINE_TYPE_INACTIVATED, epitope1, sizeof(epitope1), "V1", &id1);
    vaccine_create_entry(vaccine, VACCINE_TYPE_INACTIVATED, epitope2, sizeof(epitope2), "V2", &id2);

    // Administration succeeds
    vaccine_administer(vaccine, id1);
    vaccine_administer(vaccine, id2);

    uint32_t active_ids[10];
    size_t count;
    int result = vaccine_get_active_vaccines(vaccine, active_ids, 10, &count);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(count, 2u);  // Two active vaccines
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(ImmuneVaccineTest, GetStats) {
    vaccine_stats_t stats;
    int result = vaccine_get_stats(vaccine, &stats);
    EXPECT_EQ(result, 0);
}

TEST_F(ImmuneVaccineTest, StatsTrackAdministrations) {
    uint8_t epitope[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    uint32_t vaccine_id;
    vaccine_create_entry(vaccine, VACCINE_TYPE_INACTIVATED, epitope, sizeof(epitope),
                         "Test", &vaccine_id);
    vaccine_administer(vaccine, vaccine_id);  // Succeeds

    vaccine_stats_t stats;
    int result = vaccine_get_stats(vaccine, &stats);
    EXPECT_EQ(result, 0);
    // total_vaccines incremented on successful administration
    EXPECT_EQ(stats.total_vaccines, 1u);
}

/* ============================================================================
 * Update Tests
 * ============================================================================ */

TEST_F(ImmuneVaccineTest, Update) {
    int result = vaccine_update(vaccine, 1000);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * String Conversion Tests
 * ============================================================================ */

TEST_F(ImmuneVaccineTest, TypeToString) {
    EXPECT_STREQ(vaccine_type_to_string(VACCINE_TYPE_ATTENUATED), "ATTENUATED");
    EXPECT_STREQ(vaccine_type_to_string(VACCINE_TYPE_INACTIVATED), "INACTIVATED");
    EXPECT_STREQ(vaccine_type_to_string(VACCINE_TYPE_SUBUNIT), "SUBUNIT");
    EXPECT_STREQ(vaccine_type_to_string(VACCINE_TYPE_MRNA), "MRNA");
    EXPECT_STREQ(vaccine_type_to_string(VACCINE_TYPE_PASSIVE), "PASSIVE");
}

TEST_F(ImmuneVaccineTest, StatusToString) {
    EXPECT_STREQ(vaccine_status_to_string(VACCINE_STATUS_PENDING), "PENDING");
    EXPECT_STREQ(vaccine_status_to_string(VACCINE_STATUS_ADMINISTERED), "ADMINISTERED");
    EXPECT_STREQ(vaccine_status_to_string(VACCINE_STATUS_BOOSTED), "BOOSTED");
    EXPECT_STREQ(vaccine_status_to_string(VACCINE_STATUS_EXPIRED), "EXPIRED");
}

/* ============================================================================
 * Edge Cases
 * ============================================================================ */

TEST_F(ImmuneVaccineTest, NullSystemChecks) {
    uint8_t epitope[] = {0x01, 0x02, 0x03};
    uint32_t id;
    EXPECT_NE(vaccine_create_entry(nullptr, VACCINE_TYPE_INACTIVATED, epitope, sizeof(epitope), "T", &id), 0);
    EXPECT_NE(vaccine_administer(nullptr, 1), 0);
}

TEST_F(ImmuneVaccineTest, MaxVaccineCapacity) {
    for (size_t i = 0; i < config.max_vaccines + 5; i++) {
        uint8_t epitope[4];
        epitope[0] = (uint8_t)(i >> 24);
        epitope[1] = (uint8_t)(i >> 16);
        epitope[2] = (uint8_t)(i >> 8);
        epitope[3] = (uint8_t)(i);
        vaccine_create_entry(vaccine, VACCINE_TYPE_INACTIVATED, epitope, sizeof(epitope), "V", nullptr);
    }

    EXPECT_LE(vaccine->vaccine_count, config.max_vaccines);
}
