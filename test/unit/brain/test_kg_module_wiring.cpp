/**
 * @file test_kg_module_wiring.cpp
 * @brief Unit tests for KG Module Wiring Descriptor
 *
 * WHAT: Comprehensive test suite for kg_module_wiring API
 * WHY:  Verify correct behavior of module wiring descriptors
 * HOW:  Unit tests using GTest framework covering all API functions
 *
 * @author NIMCP Development Team
 * @date 2026-01-16
 */

#include <gtest/gtest.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>

extern "C" {
#include "core/brain/nimcp_kg_module_wiring.h"
}

/*=============================================================================
 * Test Fixture
 *=============================================================================*/

class KgModuleWiringTest : public ::testing::Test {
protected:
    kg_module_wiring_t* wiring = nullptr;

    void SetUp() override {
        wiring = kg_module_wiring_create("test_module", "TEST");
        ASSERT_NE(wiring, nullptr);
    }

    void TearDown() override {
        if (wiring) {
            kg_module_wiring_destroy(wiring);
            wiring = nullptr;
        }
    }
};

/*=============================================================================
 * Lifecycle Tests
 *=============================================================================*/

TEST(KgModuleWiringLifecycle, CreateValid) {
    kg_module_wiring_t* w = kg_module_wiring_create("my_module", "SNN");
    ASSERT_NE(w, nullptr);
    EXPECT_STREQ(w->module_name, "my_module");
    EXPECT_STREQ(w->module_type, "SNN");
    EXPECT_EQ(w->input_count, 0u);
    EXPECT_EQ(w->output_count, 0u);
    EXPECT_EQ(w->handler_count, 0u);
    EXPECT_EQ(w->network_type, KG_WEIGHT_NONE);
    EXPECT_EQ(w->initial_weights, nullptr);
    EXPECT_EQ(w->weights_size, 0u);
    EXPECT_GT(w->creation_timestamp, 0u);
    EXPECT_EQ(w->version, 1u);
    kg_module_wiring_destroy(w);
}

TEST(KgModuleWiringLifecycle, CreateNullName) {
    kg_module_wiring_t* w = kg_module_wiring_create(nullptr, "SNN");
    EXPECT_EQ(w, nullptr);
}

TEST(KgModuleWiringLifecycle, CreateNullType) {
    kg_module_wiring_t* w = kg_module_wiring_create("my_module", nullptr);
    EXPECT_EQ(w, nullptr);
}

TEST(KgModuleWiringLifecycle, CreateEmptyName) {
    kg_module_wiring_t* w = kg_module_wiring_create("", "SNN");
    EXPECT_EQ(w, nullptr);
}

TEST(KgModuleWiringLifecycle, CreateEmptyType) {
    kg_module_wiring_t* w = kg_module_wiring_create("my_module", "");
    EXPECT_EQ(w, nullptr);
}

TEST(KgModuleWiringLifecycle, CreateNameTooLong) {
    /* Name must be < KG_WIRING_MAX_NAME_LEN (64) */
    char long_name[128];
    memset(long_name, 'a', 64);
    long_name[64] = '\0';
    kg_module_wiring_t* w = kg_module_wiring_create(long_name, "SNN");
    EXPECT_EQ(w, nullptr);
}

TEST(KgModuleWiringLifecycle, CreateTypeTooLong) {
    /* Type must be < KG_WIRING_MAX_TYPE_LEN (32) */
    char long_type[64];
    memset(long_type, 'a', 32);
    long_type[32] = '\0';
    kg_module_wiring_t* w = kg_module_wiring_create("my_module", long_type);
    EXPECT_EQ(w, nullptr);
}

TEST(KgModuleWiringLifecycle, CreateMaxValidName) {
    /* Name of exactly 63 chars should work */
    char max_name[64];
    memset(max_name, 'a', 63);
    max_name[63] = '\0';
    kg_module_wiring_t* w = kg_module_wiring_create(max_name, "SNN");
    ASSERT_NE(w, nullptr);
    EXPECT_STREQ(w->module_name, max_name);
    kg_module_wiring_destroy(w);
}

TEST(KgModuleWiringLifecycle, DestroyNull) {
    /* Should not crash */
    kg_module_wiring_destroy(nullptr);
}

/*=============================================================================
 * Input Registration Tests
 *=============================================================================*/

TEST_F(KgModuleWiringTest, AddInputValid) {
    int ret = kg_module_wiring_add_input(wiring, "source_mod", "MSG_TYPE", true);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(wiring->input_count, 1u);
    EXPECT_STREQ(wiring->inputs[0].source_module, "source_mod");
    EXPECT_STREQ(wiring->inputs[0].message_type, "MSG_TYPE");
    EXPECT_TRUE(wiring->inputs[0].required);
}

TEST_F(KgModuleWiringTest, AddInputOptional) {
    int ret = kg_module_wiring_add_input(wiring, "source_mod", "MSG_TYPE", false);
    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(wiring->inputs[0].required);
}

TEST_F(KgModuleWiringTest, AddInputNullWiring) {
    int ret = kg_module_wiring_add_input(nullptr, "source", "type", true);
    EXPECT_EQ(ret, -1);
}

TEST_F(KgModuleWiringTest, AddInputNullSource) {
    int ret = kg_module_wiring_add_input(wiring, nullptr, "type", true);
    EXPECT_EQ(ret, -1);
}

TEST_F(KgModuleWiringTest, AddInputNullMsgType) {
    int ret = kg_module_wiring_add_input(wiring, "source", nullptr, true);
    EXPECT_EQ(ret, -1);
}

TEST_F(KgModuleWiringTest, AddInputMultiple) {
    int ret;
    ret = kg_module_wiring_add_input(wiring, "mod1", "TYPE_A", true);
    EXPECT_EQ(ret, 0);
    ret = kg_module_wiring_add_input(wiring, "mod2", "TYPE_B", false);
    EXPECT_EQ(ret, 0);
    ret = kg_module_wiring_add_input(wiring, "mod3", "TYPE_C", true);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(wiring->input_count, 3u);
}

TEST_F(KgModuleWiringTest, AddInputDuplicateUpdates) {
    /* Adding duplicate source+msg_type should update required flag */
    int ret;
    ret = kg_module_wiring_add_input(wiring, "source", "TYPE", true);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(wiring->input_count, 1u);
    EXPECT_TRUE(wiring->inputs[0].required);

    ret = kg_module_wiring_add_input(wiring, "source", "TYPE", false);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(wiring->input_count, 1u);  /* Still 1, not 2 */
    EXPECT_FALSE(wiring->inputs[0].required);  /* Updated */
}

TEST_F(KgModuleWiringTest, AddInputSameSourceDiffType) {
    /* Same source but different msg_type should be separate entries */
    int ret;
    ret = kg_module_wiring_add_input(wiring, "source", "TYPE_A", true);
    EXPECT_EQ(ret, 0);
    ret = kg_module_wiring_add_input(wiring, "source", "TYPE_B", true);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(wiring->input_count, 2u);
}

TEST_F(KgModuleWiringTest, AddInputMaxCapacity) {
    /* Fill up to max inputs (32), then fail on next */
    int ret;
    char source[32];
    for (uint32_t i = 0; i < KG_WIRING_MAX_INPUTS; i++) {
        snprintf(source, sizeof(source), "source_%u", i);
        ret = kg_module_wiring_add_input(wiring, source, "TYPE", true);
        EXPECT_EQ(ret, 0);
    }
    EXPECT_EQ(wiring->input_count, KG_WIRING_MAX_INPUTS);

    /* Next should fail */
    ret = kg_module_wiring_add_input(wiring, "overflow_source", "TYPE", true);
    EXPECT_EQ(ret, -1);
    EXPECT_EQ(wiring->input_count, KG_WIRING_MAX_INPUTS);
}

/*=============================================================================
 * Output Registration Tests
 *=============================================================================*/

TEST_F(KgModuleWiringTest, AddOutputValid) {
    int ret = kg_module_wiring_add_output(wiring, "OUTPUT_MSG", "Output description");
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(wiring->output_count, 1u);
    EXPECT_STREQ(wiring->outputs[0].message_type, "OUTPUT_MSG");
    EXPECT_STREQ(wiring->outputs[0].description, "Output description");
}

TEST_F(KgModuleWiringTest, AddOutputNullDescription) {
    /* NULL description is allowed */
    int ret = kg_module_wiring_add_output(wiring, "OUTPUT_MSG", nullptr);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(wiring->output_count, 1u);
    EXPECT_STREQ(wiring->outputs[0].description, "");
}

TEST_F(KgModuleWiringTest, AddOutputNullWiring) {
    int ret = kg_module_wiring_add_output(nullptr, "OUTPUT_MSG", "desc");
    EXPECT_EQ(ret, -1);
}

TEST_F(KgModuleWiringTest, AddOutputNullMsgType) {
    int ret = kg_module_wiring_add_output(wiring, nullptr, "desc");
    EXPECT_EQ(ret, -1);
}

TEST_F(KgModuleWiringTest, AddOutputDuplicateUpdates) {
    int ret;
    ret = kg_module_wiring_add_output(wiring, "OUTPUT_MSG", "Description 1");
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(wiring->output_count, 1u);

    ret = kg_module_wiring_add_output(wiring, "OUTPUT_MSG", "Description 2");
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(wiring->output_count, 1u);  /* Still 1 */
    EXPECT_STREQ(wiring->outputs[0].description, "Description 2");  /* Updated */
}

TEST_F(KgModuleWiringTest, AddOutputMaxCapacity) {
    int ret;
    char msg_type[64];
    for (uint32_t i = 0; i < KG_WIRING_MAX_OUTPUTS; i++) {
        snprintf(msg_type, sizeof(msg_type), "OUTPUT_%u", i);
        ret = kg_module_wiring_add_output(wiring, msg_type, "desc");
        EXPECT_EQ(ret, 0);
    }
    EXPECT_EQ(wiring->output_count, KG_WIRING_MAX_OUTPUTS);

    ret = kg_module_wiring_add_output(wiring, "OVERFLOW", "desc");
    EXPECT_EQ(ret, -1);
}

/*=============================================================================
 * Handler Registration Tests
 *=============================================================================*/

TEST_F(KgModuleWiringTest, AddHandlerValid) {
    int ret = kg_module_wiring_add_handler(wiring, "HANDLER_MSG", 100);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(wiring->handler_count, 1u);
    EXPECT_STREQ(wiring->handlers[0].message_type, "HANDLER_MSG");
    EXPECT_EQ(wiring->handlers[0].priority, 100u);
}

TEST_F(KgModuleWiringTest, AddHandlerNullWiring) {
    int ret = kg_module_wiring_add_handler(nullptr, "MSG", 100);
    EXPECT_EQ(ret, -1);
}

TEST_F(KgModuleWiringTest, AddHandlerNullMsgType) {
    int ret = kg_module_wiring_add_handler(wiring, nullptr, 100);
    EXPECT_EQ(ret, -1);
}

TEST_F(KgModuleWiringTest, AddHandlerDuplicateUpdatesPriority) {
    int ret;
    ret = kg_module_wiring_add_handler(wiring, "MSG", 100);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(wiring->handlers[0].priority, 100u);

    ret = kg_module_wiring_add_handler(wiring, "MSG", 200);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(wiring->handler_count, 1u);  /* Still 1 */
    EXPECT_EQ(wiring->handlers[0].priority, 200u);  /* Updated */
}

TEST_F(KgModuleWiringTest, AddHandlerMaxCapacity) {
    int ret;
    char msg_type[64];
    for (uint32_t i = 0; i < KG_WIRING_MAX_HANDLERS; i++) {
        snprintf(msg_type, sizeof(msg_type), "HANDLER_%u", i);
        ret = kg_module_wiring_add_handler(wiring, msg_type, i);
        EXPECT_EQ(ret, 0);
    }
    EXPECT_EQ(wiring->handler_count, KG_WIRING_MAX_HANDLERS);

    ret = kg_module_wiring_add_handler(wiring, "OVERFLOW", 999);
    EXPECT_EQ(ret, -1);
}

/*=============================================================================
 * Weight State Tests
 *=============================================================================*/

TEST_F(KgModuleWiringTest, SetWeightsValid) {
    uint8_t weights[] = {1, 2, 3, 4, 5};
    int ret = kg_module_wiring_set_weights(wiring, KG_WEIGHT_SNN, weights, sizeof(weights));
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(wiring->network_type, KG_WEIGHT_SNN);
    ASSERT_NE(wiring->initial_weights, nullptr);
    EXPECT_EQ(wiring->weights_size, sizeof(weights));
    EXPECT_EQ(memcmp(wiring->initial_weights, weights, sizeof(weights)), 0);
}

TEST_F(KgModuleWiringTest, SetWeightsNullWiring) {
    uint8_t weights[] = {1, 2, 3};
    int ret = kg_module_wiring_set_weights(nullptr, KG_WEIGHT_SNN, weights, sizeof(weights));
    EXPECT_EQ(ret, -1);
}

TEST_F(KgModuleWiringTest, SetWeightsNullData) {
    /* NULL weights with size 0 should just set the type */
    int ret = kg_module_wiring_set_weights(wiring, KG_WEIGHT_CNN, nullptr, 0);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(wiring->network_type, KG_WEIGHT_CNN);
    EXPECT_EQ(wiring->initial_weights, nullptr);
    EXPECT_EQ(wiring->weights_size, 0u);
}

TEST_F(KgModuleWiringTest, SetWeightsReplaceExisting) {
    uint8_t weights1[] = {1, 2, 3};
    uint8_t weights2[] = {4, 5, 6, 7, 8};

    int ret = kg_module_wiring_set_weights(wiring, KG_WEIGHT_SNN, weights1, sizeof(weights1));
    EXPECT_EQ(ret, 0);

    ret = kg_module_wiring_set_weights(wiring, KG_WEIGHT_LNN, weights2, sizeof(weights2));
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(wiring->network_type, KG_WEIGHT_LNN);
    EXPECT_EQ(wiring->weights_size, sizeof(weights2));
    EXPECT_EQ(memcmp(wiring->initial_weights, weights2, sizeof(weights2)), 0);
}

TEST_F(KgModuleWiringTest, SetWeightsClearExisting) {
    uint8_t weights[] = {1, 2, 3};
    int ret = kg_module_wiring_set_weights(wiring, KG_WEIGHT_SNN, weights, sizeof(weights));
    EXPECT_EQ(ret, 0);
    ASSERT_NE(wiring->initial_weights, nullptr);

    /* Clear by setting NULL */
    ret = kg_module_wiring_set_weights(wiring, KG_WEIGHT_NONE, nullptr, 0);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(wiring->network_type, KG_WEIGHT_NONE);
    EXPECT_EQ(wiring->initial_weights, nullptr);
    EXPECT_EQ(wiring->weights_size, 0u);
}

/*=============================================================================
 * Metadata Tests
 *=============================================================================*/

TEST_F(KgModuleWiringTest, SetMetadataValid) {
    int ret = kg_module_wiring_set_metadata(wiring, "Author Name", "Category", "Description text");
    EXPECT_EQ(ret, 0);
    EXPECT_STREQ(wiring->metadata.author, "Author Name");
    EXPECT_STREQ(wiring->metadata.category, "Category");
    EXPECT_STREQ(wiring->metadata.description, "Description text");
}

TEST_F(KgModuleWiringTest, SetMetadataNullFields) {
    /* NULL for any field should skip that field */
    int ret = kg_module_wiring_set_metadata(wiring, "Author", nullptr, nullptr);
    EXPECT_EQ(ret, 0);
    EXPECT_STREQ(wiring->metadata.author, "Author");
    /* category and description should remain empty/unchanged */
}

TEST_F(KgModuleWiringTest, SetMetadataNullWiring) {
    int ret = kg_module_wiring_set_metadata(nullptr, "Author", "Cat", "Desc");
    EXPECT_EQ(ret, -1);
}

TEST_F(KgModuleWiringTest, AddMetadataEntryValid) {
    int ret = kg_module_wiring_add_metadata_entry(wiring, "key1", "value1");
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(wiring->metadata.entry_count, 1u);
    EXPECT_STREQ(wiring->metadata.entries[0].key, "key1");
    EXPECT_STREQ(wiring->metadata.entries[0].value, "value1");
}

TEST_F(KgModuleWiringTest, AddMetadataEntryNullParams) {
    int ret;
    ret = kg_module_wiring_add_metadata_entry(nullptr, "key", "value");
    EXPECT_EQ(ret, -1);
    ret = kg_module_wiring_add_metadata_entry(wiring, nullptr, "value");
    EXPECT_EQ(ret, -1);
    ret = kg_module_wiring_add_metadata_entry(wiring, "key", nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(KgModuleWiringTest, AddMetadataEntryDuplicateUpdates) {
    int ret;
    ret = kg_module_wiring_add_metadata_entry(wiring, "key", "value1");
    EXPECT_EQ(ret, 0);

    ret = kg_module_wiring_add_metadata_entry(wiring, "key", "value2");
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(wiring->metadata.entry_count, 1u);  /* Still 1 */
    EXPECT_STREQ(wiring->metadata.entries[0].value, "value2");  /* Updated */
}

TEST_F(KgModuleWiringTest, AddMetadataEntryMaxCapacity) {
    int ret;
    char key[32];
    for (uint32_t i = 0; i < KG_WIRING_MAX_METADATA; i++) {
        snprintf(key, sizeof(key), "key_%u", i);
        ret = kg_module_wiring_add_metadata_entry(wiring, key, "value");
        EXPECT_EQ(ret, 0);
    }
    EXPECT_EQ(wiring->metadata.entry_count, KG_WIRING_MAX_METADATA);

    ret = kg_module_wiring_add_metadata_entry(wiring, "overflow_key", "value");
    EXPECT_EQ(ret, -1);
}

TEST_F(KgModuleWiringTest, SetVersionValid) {
    int ret = kg_module_wiring_set_version(wiring, 2, 5, 10);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(wiring->metadata.version_major, 2u);
    EXPECT_EQ(wiring->metadata.version_minor, 5u);
    EXPECT_EQ(wiring->metadata.version_patch, 10u);

    /* Check encoded version: (2 << 32) | (5 << 16) | 10 */
    uint64_t expected = ((uint64_t)2 << 32) | ((uint64_t)5 << 16) | 10;
    EXPECT_EQ(wiring->version, expected);
}

TEST_F(KgModuleWiringTest, SetVersionNullWiring) {
    int ret = kg_module_wiring_set_version(nullptr, 1, 0, 0);
    EXPECT_EQ(ret, -1);
}

/*=============================================================================
 * Query API Tests
 *=============================================================================*/

TEST_F(KgModuleWiringTest, HasInputFound) {
    kg_module_wiring_add_input(wiring, "source", "MSG_TYPE", true);
    EXPECT_TRUE(kg_module_wiring_has_input(wiring, "source", "MSG_TYPE"));
}

TEST_F(KgModuleWiringTest, HasInputNotFound) {
    kg_module_wiring_add_input(wiring, "source", "MSG_TYPE", true);
    EXPECT_FALSE(kg_module_wiring_has_input(wiring, "other", "MSG_TYPE"));
    EXPECT_FALSE(kg_module_wiring_has_input(wiring, "source", "OTHER"));
}

TEST_F(KgModuleWiringTest, HasInputNullMsgTypeMatchesAny) {
    kg_module_wiring_add_input(wiring, "source", "MSG_TYPE", true);
    /* NULL msg_type should match any message type from that source */
    EXPECT_TRUE(kg_module_wiring_has_input(wiring, "source", nullptr));
}

TEST_F(KgModuleWiringTest, HasInputNullParams) {
    EXPECT_FALSE(kg_module_wiring_has_input(nullptr, "source", "type"));
    EXPECT_FALSE(kg_module_wiring_has_input(wiring, nullptr, "type"));
}

TEST_F(KgModuleWiringTest, HasOutputFound) {
    kg_module_wiring_add_output(wiring, "OUTPUT_MSG", "desc");
    EXPECT_TRUE(kg_module_wiring_has_output(wiring, "OUTPUT_MSG"));
}

TEST_F(KgModuleWiringTest, HasOutputNotFound) {
    kg_module_wiring_add_output(wiring, "OUTPUT_MSG", "desc");
    EXPECT_FALSE(kg_module_wiring_has_output(wiring, "OTHER"));
}

TEST_F(KgModuleWiringTest, HasOutputNullParams) {
    EXPECT_FALSE(kg_module_wiring_has_output(nullptr, "type"));
    EXPECT_FALSE(kg_module_wiring_has_output(wiring, nullptr));
}

TEST_F(KgModuleWiringTest, HasHandlerFound) {
    kg_module_wiring_add_handler(wiring, "HANDLER_MSG", 100);
    EXPECT_TRUE(kg_module_wiring_has_handler(wiring, "HANDLER_MSG"));
}

TEST_F(KgModuleWiringTest, HasHandlerNotFound) {
    kg_module_wiring_add_handler(wiring, "HANDLER_MSG", 100);
    EXPECT_FALSE(kg_module_wiring_has_handler(wiring, "OTHER"));
}

TEST_F(KgModuleWiringTest, HasHandlerNullParams) {
    EXPECT_FALSE(kg_module_wiring_has_handler(nullptr, "type"));
    EXPECT_FALSE(kg_module_wiring_has_handler(wiring, nullptr));
}

TEST_F(KgModuleWiringTest, GetHandlerPriorityFound) {
    kg_module_wiring_add_handler(wiring, "HANDLER_MSG", 150);
    uint32_t priority = kg_module_wiring_get_handler_priority(wiring, "HANDLER_MSG");
    EXPECT_EQ(priority, 150u);
}

TEST_F(KgModuleWiringTest, GetHandlerPriorityNotFound) {
    kg_module_wiring_add_handler(wiring, "HANDLER_MSG", 150);
    uint32_t priority = kg_module_wiring_get_handler_priority(wiring, "OTHER");
    EXPECT_EQ(priority, 0u);
}

TEST_F(KgModuleWiringTest, GetHandlerPriorityNullParams) {
    EXPECT_EQ(kg_module_wiring_get_handler_priority(nullptr, "type"), 0u);
    EXPECT_EQ(kg_module_wiring_get_handler_priority(wiring, nullptr), 0u);
}

/*=============================================================================
 * Validation Tests
 *=============================================================================*/

TEST_F(KgModuleWiringTest, ValidateValid) {
    char error_buf[256] = {0};
    int ret = kg_module_wiring_validate(wiring, error_buf, sizeof(error_buf));
    EXPECT_EQ(ret, 0);
}

TEST_F(KgModuleWiringTest, ValidateNullWiring) {
    char error_buf[256] = {0};
    int ret = kg_module_wiring_validate(nullptr, error_buf, sizeof(error_buf));
    EXPECT_EQ(ret, -1);
    EXPECT_STREQ(error_buf, "Wiring descriptor is NULL");
}

TEST_F(KgModuleWiringTest, ValidateNullErrorBuf) {
    /* Should work even without error buffer */
    int ret = kg_module_wiring_validate(wiring, nullptr, 0);
    EXPECT_EQ(ret, 0);
}

TEST_F(KgModuleWiringTest, ValidateWithInputsOutputsHandlers) {
    kg_module_wiring_add_input(wiring, "source", "MSG", true);
    kg_module_wiring_add_output(wiring, "OUT", "desc");
    kg_module_wiring_add_handler(wiring, "HANDLER", 100);

    char error_buf[256] = {0};
    int ret = kg_module_wiring_validate(wiring, error_buf, sizeof(error_buf));
    EXPECT_EQ(ret, 0);
}

TEST_F(KgModuleWiringTest, ValidateWeightConsistencyPointerNoSize) {
    /* Set weights with pointer but then manually corrupt size */
    uint8_t weights[] = {1, 2, 3};
    kg_module_wiring_set_weights(wiring, KG_WEIGHT_SNN, weights, sizeof(weights));
    wiring->weights_size = 0;  /* Corrupt */

    char error_buf[256] = {0};
    int ret = kg_module_wiring_validate(wiring, error_buf, sizeof(error_buf));
    EXPECT_EQ(ret, -1);
    EXPECT_NE(strstr(error_buf, "Weights pointer set but size is zero"), nullptr);
}

TEST_F(KgModuleWiringTest, ValidateWeightConsistencySizeNoPointer) {
    /* Set weights then manually corrupt */
    uint8_t weights[] = {1, 2, 3};
    kg_module_wiring_set_weights(wiring, KG_WEIGHT_SNN, weights, sizeof(weights));
    void* saved = wiring->initial_weights;
    wiring->initial_weights = nullptr;  /* Corrupt */

    char error_buf[256] = {0};
    int ret = kg_module_wiring_validate(wiring, error_buf, sizeof(error_buf));
    EXPECT_EQ(ret, -1);
    EXPECT_NE(strstr(error_buf, "Weights size set but pointer is NULL"), nullptr);

    /* Restore for proper cleanup */
    wiring->initial_weights = saved;
}

/*=============================================================================
 * String Conversion Tests
 *=============================================================================*/

TEST(KgModuleWiringStrings, WeightTypeToString) {
    EXPECT_STREQ(kg_weight_type_to_string(KG_WEIGHT_NONE), "NONE");
    EXPECT_STREQ(kg_weight_type_to_string(KG_WEIGHT_SNN), "SNN");
    EXPECT_STREQ(kg_weight_type_to_string(KG_WEIGHT_LNN), "LNN");
    EXPECT_STREQ(kg_weight_type_to_string(KG_WEIGHT_CNN), "CNN");
    EXPECT_STREQ(kg_weight_type_to_string(KG_WEIGHT_RNN), "RNN");
    EXPECT_STREQ(kg_weight_type_to_string(KG_WEIGHT_TRANSFORMER), "TRANSFORMER");
    EXPECT_STREQ(kg_weight_type_to_string(KG_WEIGHT_GNN), "GNN");
    EXPECT_STREQ(kg_weight_type_to_string(KG_WEIGHT_HYBRID), "HYBRID");
    EXPECT_STREQ(kg_weight_type_to_string(KG_WEIGHT_CUSTOM), "CUSTOM");
}

TEST(KgModuleWiringStrings, WeightTypeToStringUnknown) {
    EXPECT_STREQ(kg_weight_type_to_string((kg_weight_type_t)999), "UNKNOWN");
    EXPECT_STREQ(kg_weight_type_to_string((kg_weight_type_t)-1), "UNKNOWN");
}

TEST(KgModuleWiringStrings, WeightTypeFromString) {
    EXPECT_EQ(kg_weight_type_from_string("NONE"), KG_WEIGHT_NONE);
    EXPECT_EQ(kg_weight_type_from_string("SNN"), KG_WEIGHT_SNN);
    EXPECT_EQ(kg_weight_type_from_string("LNN"), KG_WEIGHT_LNN);
    EXPECT_EQ(kg_weight_type_from_string("CNN"), KG_WEIGHT_CNN);
    EXPECT_EQ(kg_weight_type_from_string("RNN"), KG_WEIGHT_RNN);
    EXPECT_EQ(kg_weight_type_from_string("TRANSFORMER"), KG_WEIGHT_TRANSFORMER);
    EXPECT_EQ(kg_weight_type_from_string("GNN"), KG_WEIGHT_GNN);
    EXPECT_EQ(kg_weight_type_from_string("HYBRID"), KG_WEIGHT_HYBRID);
    EXPECT_EQ(kg_weight_type_from_string("CUSTOM"), KG_WEIGHT_CUSTOM);
}

TEST(KgModuleWiringStrings, WeightTypeFromStringCaseInsensitive) {
    EXPECT_EQ(kg_weight_type_from_string("snn"), KG_WEIGHT_SNN);
    EXPECT_EQ(kg_weight_type_from_string("Snn"), KG_WEIGHT_SNN);
    EXPECT_EQ(kg_weight_type_from_string("transformer"), KG_WEIGHT_TRANSFORMER);
}

TEST(KgModuleWiringStrings, WeightTypeFromStringNull) {
    EXPECT_EQ(kg_weight_type_from_string(nullptr), KG_WEIGHT_NONE);
}

TEST(KgModuleWiringStrings, WeightTypeFromStringUnknown) {
    EXPECT_EQ(kg_weight_type_from_string("INVALID"), KG_WEIGHT_NONE);
    EXPECT_EQ(kg_weight_type_from_string(""), KG_WEIGHT_NONE);
}

/*=============================================================================
 * Integration Tests
 *=============================================================================*/

TEST(KgModuleWiringIntegration, FullWiringWorkflow) {
    /* Complete workflow test */
    kg_module_wiring_t* w = kg_module_wiring_create("prefrontal_cortex", "COGNITIVE");
    ASSERT_NE(w, nullptr);

    /* Set metadata */
    int ret = kg_module_wiring_set_metadata(w, "NIMCP Team", "Brain", "Prefrontal cortex module");
    EXPECT_EQ(ret, 0);

    ret = kg_module_wiring_set_version(w, 1, 2, 3);
    EXPECT_EQ(ret, 0);

    ret = kg_module_wiring_add_metadata_entry(w, "region", "frontal");
    EXPECT_EQ(ret, 0);

    /* Add inputs */
    ret = kg_module_wiring_add_input(w, "hippocampus", "MEMORY_QUERY", true);
    EXPECT_EQ(ret, 0);
    ret = kg_module_wiring_add_input(w, "basal_ganglia", "REWARD_SIGNAL", false);
    EXPECT_EQ(ret, 0);

    /* Add outputs */
    ret = kg_module_wiring_add_output(w, "DECISION_OUT", "Executive decisions");
    EXPECT_EQ(ret, 0);
    ret = kg_module_wiring_add_output(w, "WORKING_MEM", "Working memory updates");
    EXPECT_EQ(ret, 0);

    /* Add handlers */
    ret = kg_module_wiring_add_handler(w, "MEMORY_QUERY", 100);
    EXPECT_EQ(ret, 0);
    ret = kg_module_wiring_add_handler(w, "DECISION_REQUEST", 200);
    EXPECT_EQ(ret, 0);

    /* Set weights */
    float weights[] = {0.1f, 0.2f, 0.3f, 0.4f};
    ret = kg_module_wiring_set_weights(w, KG_WEIGHT_SNN, weights, sizeof(weights));
    EXPECT_EQ(ret, 0);

    /* Validate */
    char error_buf[256] = {0};
    ret = kg_module_wiring_validate(w, error_buf, sizeof(error_buf));
    EXPECT_EQ(ret, 0);

    /* Query checks */
    EXPECT_TRUE(kg_module_wiring_has_input(w, "hippocampus", "MEMORY_QUERY"));
    EXPECT_TRUE(kg_module_wiring_has_input(w, "basal_ganglia", nullptr));  /* Any from source */
    EXPECT_FALSE(kg_module_wiring_has_input(w, "amygdala", nullptr));

    EXPECT_TRUE(kg_module_wiring_has_output(w, "DECISION_OUT"));
    EXPECT_FALSE(kg_module_wiring_has_output(w, "OTHER"));

    EXPECT_TRUE(kg_module_wiring_has_handler(w, "MEMORY_QUERY"));
    EXPECT_EQ(kg_module_wiring_get_handler_priority(w, "DECISION_REQUEST"), 200u);

    /* Verify counts */
    EXPECT_EQ(w->input_count, 2u);
    EXPECT_EQ(w->output_count, 2u);
    EXPECT_EQ(w->handler_count, 2u);

    kg_module_wiring_destroy(w);
}
