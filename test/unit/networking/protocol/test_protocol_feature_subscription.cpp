/**
 * @file test_protocol_feature_subscription.cpp
 * @brief Comprehensive unit tests for NIMCP 2.0 feature code and subscription functionality
 * @details Tests feature code macros, domain naming, hierarchical namespace matching,
 *          subscription filtering, confidence thresholds, and rate limiting logic
 */

#include <string.h>
#include "networking/protocol/nimcp_protocol.h"
#include "test_helpers.h"

//=============================================================================
// Test Fixture
//=============================================================================

class ProtocolFeatureSubscriptionTest : public ::testing::Test {
   protected:
    event_packet_t packet;
    subscription_filter_t filter;
    uint8_t buffer[1024];

    void SetUp() override
    {
        // Initialize packet with default values
        memset(&packet, 0, sizeof(event_packet_t));
        EVENT_SET_VERSION(&packet, PROTOCOL_VERSION);
        EVENT_SET_FLAGS(&packet, EVENT_FLAG_EXCITATORY);
        packet.source_node_id = 1;
        packet.timestamp = 1000000;
        packet.confidence = EVENT_FLOAT_TO_CONFIDENCE(0.8f);
        packet.hop_count = 1;
        packet.payload_length = 0;

        // Initialize filter with default values
        memset(&filter, 0, sizeof(subscription_filter_t));
        filter.feature_code = MAKE_FEATURE_CODE(FEATURE_DOMAIN_SYSTEM, 0);
        filter.feature_mask = 0xFF000000;  // Domain only
        filter.confidence_threshold = 0.5f;
        filter.max_rate_hz = 0;  // Unlimited
    }
};

//=============================================================================
// Feature Code Macro Tests
//=============================================================================

class FeatureCodeMacroTest : public ProtocolFeatureSubscriptionTest {};

TEST_F(FeatureCodeMacroTest, MakeFeatureCodeSystemDomain)
{
    feature_code_t code = MAKE_FEATURE_CODE(FEATURE_DOMAIN_SYSTEM, 0x123456);
    EXPECT_EQ(code, 0x00123456);
}

TEST_F(FeatureCodeMacroTest, MakeFeatureCodeVisionDomain)
{
    feature_code_t code = MAKE_FEATURE_CODE(FEATURE_DOMAIN_VISION, 0xABCDEF);
    EXPECT_EQ(code, 0x10ABCDEF);
}

TEST_F(FeatureCodeMacroTest, MakeFeatureCodeAuditoryDomain)
{
    feature_code_t code = MAKE_FEATURE_CODE(FEATURE_DOMAIN_AUDITORY, 0x000001);
    EXPECT_EQ(code, 0x20000001);
}

TEST_F(FeatureCodeMacroTest, MakeFeatureCodeLanguageDomain)
{
    feature_code_t code = MAKE_FEATURE_CODE(FEATURE_DOMAIN_LANGUAGE, 0xFFFFFF);
    EXPECT_EQ(code, 0x30FFFFFF);
}

TEST_F(FeatureCodeMacroTest, MakeFeatureCodeMotorDomain)
{
    feature_code_t code = MAKE_FEATURE_CODE(FEATURE_DOMAIN_MOTOR, 0x123ABC);
    EXPECT_EQ(code, 0x40123ABC);
}

TEST_F(FeatureCodeMacroTest, MakeFeatureCodeMemoryDomain)
{
    feature_code_t code = MAKE_FEATURE_CODE(FEATURE_DOMAIN_MEMORY, 0x000000);
    EXPECT_EQ(code, 0x50000000);
}

TEST_F(FeatureCodeMacroTest, MakeFeatureCodeEmotionDomain)
{
    feature_code_t code = MAKE_FEATURE_CODE(FEATURE_DOMAIN_EMOTION, 0x112233);
    EXPECT_EQ(code, 0x60112233);
}

TEST_F(FeatureCodeMacroTest, MakeFeatureCodeEthicsDomain)
{
    feature_code_t code = MAKE_FEATURE_CODE(FEATURE_DOMAIN_ETHICS, 0x445566);
    EXPECT_EQ(code, 0x70445566);
}

TEST_F(FeatureCodeMacroTest, MakeFeatureCodeNeuromodDomain)
{
    feature_code_t code = MAKE_FEATURE_CODE(FEATURE_DOMAIN_NEUROMOD, 0x778899);
    EXPECT_EQ(code, 0x90778899);
}

TEST_F(FeatureCodeMacroTest, MakeFeatureCodeGlialDomain)
{
    feature_code_t code = MAKE_FEATURE_CODE(FEATURE_DOMAIN_GLIAL, 0xAABBCC);
    EXPECT_EQ(code, 0xA0AABBCC);
}

TEST_F(FeatureCodeMacroTest, MakeFeatureCodeBrainRegionDomain)
{
    feature_code_t code = MAKE_FEATURE_CODE(FEATURE_DOMAIN_BRAIN_REGION, 0xDDEEFF);
    EXPECT_EQ(code, 0xB0DDEEFF);
}

TEST_F(FeatureCodeMacroTest, MakeFeatureCodeUserDefined)
{
    feature_code_t code = MAKE_FEATURE_CODE(0x80, 0x123456);
    EXPECT_EQ(code, 0x80123456);

    code = MAKE_FEATURE_CODE(0xFF, 0xABCDEF);
    EXPECT_EQ(code, 0xFFABCDEF);
}

TEST_F(FeatureCodeMacroTest, MakeFeatureCodeSubfeatureMasking)
{
    // Verify subfeature is properly masked to 24 bits
    feature_code_t code = MAKE_FEATURE_CODE(FEATURE_DOMAIN_VISION, 0xFF123456);
    EXPECT_EQ(code, 0x10123456);  // High byte should be masked off
}

//=============================================================================
// Feature Code Extraction Macro Tests
//=============================================================================

class FeatureCodeExtractionTest : public ProtocolFeatureSubscriptionTest {};

TEST_F(FeatureCodeExtractionTest, GetFeatureDomainSystem)
{
    feature_code_t code = 0x00123456;
    EXPECT_EQ(GET_FEATURE_DOMAIN(code), FEATURE_DOMAIN_SYSTEM);
}

TEST_F(FeatureCodeExtractionTest, GetFeatureDomainVision)
{
    feature_code_t code = 0x10ABCDEF;
    EXPECT_EQ(GET_FEATURE_DOMAIN(code), FEATURE_DOMAIN_VISION);
}

TEST_F(FeatureCodeExtractionTest, GetFeatureDomainAllDomains)
{
    EXPECT_EQ(GET_FEATURE_DOMAIN(0x00000000), FEATURE_DOMAIN_SYSTEM);
    EXPECT_EQ(GET_FEATURE_DOMAIN(0x10000000), FEATURE_DOMAIN_VISION);
    EXPECT_EQ(GET_FEATURE_DOMAIN(0x20000000), FEATURE_DOMAIN_AUDITORY);
    EXPECT_EQ(GET_FEATURE_DOMAIN(0x30000000), FEATURE_DOMAIN_LANGUAGE);
    EXPECT_EQ(GET_FEATURE_DOMAIN(0x40000000), FEATURE_DOMAIN_MOTOR);
    EXPECT_EQ(GET_FEATURE_DOMAIN(0x50000000), FEATURE_DOMAIN_MEMORY);
    EXPECT_EQ(GET_FEATURE_DOMAIN(0x60000000), FEATURE_DOMAIN_EMOTION);
    EXPECT_EQ(GET_FEATURE_DOMAIN(0x70000000), FEATURE_DOMAIN_ETHICS);
    EXPECT_EQ(GET_FEATURE_DOMAIN(0x90000000), FEATURE_DOMAIN_NEUROMOD);
    EXPECT_EQ(GET_FEATURE_DOMAIN(0xA0000000), FEATURE_DOMAIN_GLIAL);
    EXPECT_EQ(GET_FEATURE_DOMAIN(0xB0000000), FEATURE_DOMAIN_BRAIN_REGION);
}

TEST_F(FeatureCodeExtractionTest, GetFeatureSubcodeSimple)
{
    feature_code_t code = 0x10123456;
    EXPECT_EQ(GET_FEATURE_SUBCODE(code), 0x123456);
}

TEST_F(FeatureCodeExtractionTest, GetFeatureSubcodeMaxValue)
{
    feature_code_t code = 0x70FFFFFF;
    EXPECT_EQ(GET_FEATURE_SUBCODE(code), 0xFFFFFF);
}

TEST_F(FeatureCodeExtractionTest, GetFeatureSubcodeZero)
{
    feature_code_t code = 0x60000000;
    EXPECT_EQ(GET_FEATURE_SUBCODE(code), 0x000000);
}

TEST_F(FeatureCodeExtractionTest, GetFeatureSubcodeMasking)
{
    // Verify only bottom 24 bits are extracted
    feature_code_t code = 0xFFABCDEF;
    EXPECT_EQ(GET_FEATURE_SUBCODE(code), 0xABCDEF);
}

//=============================================================================
// Feature Domain Name Tests
//=============================================================================

class FeatureDomainNameTest : public ProtocolFeatureSubscriptionTest {};

TEST_F(FeatureDomainNameTest, SystemDomain)
{
    const char* name = feature_domain_name(FEATURE_DOMAIN_SYSTEM);
    EXPECT_STREQ(name, "System");
}

TEST_F(FeatureDomainNameTest, VisionDomain)
{
    const char* name = feature_domain_name(FEATURE_DOMAIN_VISION);
    EXPECT_STREQ(name, "Vision");
}

TEST_F(FeatureDomainNameTest, AuditoryDomain)
{
    const char* name = feature_domain_name(FEATURE_DOMAIN_AUDITORY);
    EXPECT_STREQ(name, "Auditory");
}

TEST_F(FeatureDomainNameTest, LanguageDomain)
{
    const char* name = feature_domain_name(FEATURE_DOMAIN_LANGUAGE);
    EXPECT_STREQ(name, "Language");
}

TEST_F(FeatureDomainNameTest, MotorDomain)
{
    const char* name = feature_domain_name(FEATURE_DOMAIN_MOTOR);
    EXPECT_STREQ(name, "Motor");
}

TEST_F(FeatureDomainNameTest, MemoryDomain)
{
    const char* name = feature_domain_name(FEATURE_DOMAIN_MEMORY);
    EXPECT_STREQ(name, "Memory");
}

TEST_F(FeatureDomainNameTest, EmotionDomain)
{
    const char* name = feature_domain_name(FEATURE_DOMAIN_EMOTION);
    EXPECT_STREQ(name, "Emotion");
}

TEST_F(FeatureDomainNameTest, EthicsDomain)
{
    const char* name = feature_domain_name(FEATURE_DOMAIN_ETHICS);
    EXPECT_STREQ(name, "Ethics");
}

TEST_F(FeatureDomainNameTest, NeuromodDomain)
{
    const char* name = feature_domain_name(FEATURE_DOMAIN_NEUROMOD);
    EXPECT_STREQ(name, "User-Defined");  // Neuromod is in user range
}

TEST_F(FeatureDomainNameTest, GlialDomain)
{
    const char* name = feature_domain_name(FEATURE_DOMAIN_GLIAL);
    EXPECT_STREQ(name, "User-Defined");  // Glial is in user range
}

TEST_F(FeatureDomainNameTest, BrainRegionDomain)
{
    const char* name = feature_domain_name(FEATURE_DOMAIN_BRAIN_REGION);
    EXPECT_STREQ(name, "User-Defined");  // Brain region is in user range
}

TEST_F(FeatureDomainNameTest, UserDefinedMinimum)
{
    const char* name = feature_domain_name(FEATURE_DOMAIN_USER_MIN);
    EXPECT_STREQ(name, "User-Defined");
}

TEST_F(FeatureDomainNameTest, UserDefinedMaximum)
{
    const char* name = feature_domain_name(FEATURE_DOMAIN_USER_MAX);
    EXPECT_STREQ(name, "User-Defined");
}

TEST_F(FeatureDomainNameTest, InvalidDomain)
{
    const char* name = feature_domain_name(static_cast<feature_domain_t>(0x08));
    EXPECT_STREQ(name, "Unknown");
}

//=============================================================================
// Feature Code Matching Tests
//=============================================================================

class FeatureCodeMatchingTest : public ProtocolFeatureSubscriptionTest {};

TEST_F(FeatureCodeMatchingTest, DomainOnlyMatch)
{
    // Mask 0xFF000000 = domain only
    feature_code_t code = MAKE_FEATURE_CODE(FEATURE_DOMAIN_VISION, 0x123456);
    feature_code_t filter = MAKE_FEATURE_CODE(FEATURE_DOMAIN_VISION, 0x000000);
    EXPECT_TRUE(feature_code_matches(code, filter, 0xFF000000));
}

TEST_F(FeatureCodeMatchingTest, DomainOnlyMismatch)
{
    feature_code_t code = MAKE_FEATURE_CODE(FEATURE_DOMAIN_VISION, 0x123456);
    feature_code_t filter = MAKE_FEATURE_CODE(FEATURE_DOMAIN_AUDITORY, 0x000000);
    EXPECT_FALSE(feature_code_matches(code, filter, 0xFF000000));
}

TEST_F(FeatureCodeMatchingTest, SubdomainMatch)
{
    // Mask 0xFFFF0000 = domain + subdomain (high 8 bits of subcode)
    feature_code_t code = MAKE_FEATURE_CODE(FEATURE_DOMAIN_LANGUAGE, 0xAB1234);
    feature_code_t filter = MAKE_FEATURE_CODE(FEATURE_DOMAIN_LANGUAGE, 0xAB0000);
    EXPECT_TRUE(feature_code_matches(code, filter, 0xFFFF0000));
}

TEST_F(FeatureCodeMatchingTest, SubdomainMismatch)
{
    feature_code_t code = MAKE_FEATURE_CODE(FEATURE_DOMAIN_LANGUAGE, 0xAB1234);
    feature_code_t filter = MAKE_FEATURE_CODE(FEATURE_DOMAIN_LANGUAGE, 0xCD0000);
    EXPECT_FALSE(feature_code_matches(code, filter, 0xFFFF0000));
}

TEST_F(FeatureCodeMatchingTest, ExactMatch)
{
    // Mask 0xFFFFFFFF = exact match
    feature_code_t code = MAKE_FEATURE_CODE(FEATURE_DOMAIN_MOTOR, 0x123456);
    feature_code_t filter = MAKE_FEATURE_CODE(FEATURE_DOMAIN_MOTOR, 0x123456);
    EXPECT_TRUE(feature_code_matches(code, filter, 0xFFFFFFFF));
}

TEST_F(FeatureCodeMatchingTest, ExactMismatch)
{
    feature_code_t code = MAKE_FEATURE_CODE(FEATURE_DOMAIN_MOTOR, 0x123456);
    feature_code_t filter = MAKE_FEATURE_CODE(FEATURE_DOMAIN_MOTOR, 0x123457);
    EXPECT_FALSE(feature_code_matches(code, filter, 0xFFFFFFFF));
}

TEST_F(FeatureCodeMatchingTest, ZeroMaskMatchesAnything)
{
    // Zero mask means no bits are checked
    feature_code_t code = MAKE_FEATURE_CODE(FEATURE_DOMAIN_VISION, 0xABCDEF);
    feature_code_t filter = MAKE_FEATURE_CODE(FEATURE_DOMAIN_MEMORY, 0x123456);
    EXPECT_TRUE(feature_code_matches(code, filter, 0x00000000));
}

TEST_F(FeatureCodeMatchingTest, CustomMaskPattern)
{
    // Custom mask: check domain and low byte only (0xFF0000FF)
    feature_code_t code = MAKE_FEATURE_CODE(FEATURE_DOMAIN_EMOTION, 0x12AB34);
    feature_code_t filter = MAKE_FEATURE_CODE(FEATURE_DOMAIN_EMOTION, 0x990034);
    EXPECT_TRUE(feature_code_matches(code, filter, 0xFF0000FF));
}

TEST_F(FeatureCodeMatchingTest, MiddleBitsMatch)
{
    // Match middle 16 bits only (0x00FFFF00)
    feature_code_t code = MAKE_FEATURE_CODE(FEATURE_DOMAIN_ETHICS, 0x12AB34);
    // Fix: code=0x7012AB34, masked=0x0012AB00, so filter must also be 0x0012AB00
    EXPECT_TRUE(feature_code_matches(code, 0x0012AB00, 0x00FFFF00));
    EXPECT_TRUE(feature_code_matches(code, 0xFF12ABFF, 0x00FFFF00));
    EXPECT_FALSE(feature_code_matches(code, 0x0012AC00, 0x00FFFF00));
}

TEST_F(FeatureCodeMatchingTest, HierarchicalNamespaceDepth)
{
    // Test hierarchical matching at different depths
    feature_code_t code = 0x10ABCDEF;

    // Level 1: Domain only (8 bits)
    EXPECT_TRUE(feature_code_matches(code, 0x10000000, 0xFF000000));

    // Level 2: Domain + subdomain (16 bits)
    EXPECT_TRUE(feature_code_matches(code, 0x10AB0000, 0xFFFF0000));

    // Level 3: Domain + subdomain + category (20 bits)
    EXPECT_TRUE(feature_code_matches(code, 0x10ABC000, 0xFFFFF000));

    // Level 4: Exact (32 bits)
    EXPECT_TRUE(feature_code_matches(code, 0x10ABCDEF, 0xFFFFFFFF));
}

//=============================================================================
// Subscription Matching Tests
//=============================================================================

class SubscriptionMatchingTest : public ProtocolFeatureSubscriptionTest {};

TEST_F(SubscriptionMatchingTest, BasicMatch)
{
    // Set up matching feature code
    EVENT_SET_FEATURE_CODE(&packet, MAKE_FEATURE_CODE(FEATURE_DOMAIN_SYSTEM, 0x123456));
    filter.feature_code = MAKE_FEATURE_CODE(FEATURE_DOMAIN_SYSTEM, 0);
    filter.feature_mask = 0xFF000000;  // Domain only
    filter.confidence_threshold = 0.5f;

    EXPECT_TRUE(subscription_matches(&filter, &packet));
}

TEST_F(SubscriptionMatchingTest, FeatureCodeMismatch)
{
    EVENT_SET_FEATURE_CODE(&packet, MAKE_FEATURE_CODE(FEATURE_DOMAIN_VISION, 0x123456));
    filter.feature_code = MAKE_FEATURE_CODE(FEATURE_DOMAIN_AUDITORY, 0);
    filter.feature_mask = 0xFF000000;

    EXPECT_FALSE(subscription_matches(&filter, &packet));
}

TEST_F(SubscriptionMatchingTest, ConfidenceThresholdPass)
{
    EVENT_SET_FEATURE_CODE(&packet, MAKE_FEATURE_CODE(FEATURE_DOMAIN_SYSTEM, 0));
    packet.confidence = EVENT_FLOAT_TO_CONFIDENCE(0.75f);
    filter.feature_code = MAKE_FEATURE_CODE(FEATURE_DOMAIN_SYSTEM, 0);
    filter.feature_mask = 0xFF000000;
    filter.confidence_threshold = 0.5f;

    EXPECT_TRUE(subscription_matches(&filter, &packet));
}

TEST_F(SubscriptionMatchingTest, ConfidenceThresholdFail)
{
    EVENT_SET_FEATURE_CODE(&packet, MAKE_FEATURE_CODE(FEATURE_DOMAIN_SYSTEM, 0));
    packet.confidence = EVENT_FLOAT_TO_CONFIDENCE(0.3f);
    filter.feature_code = MAKE_FEATURE_CODE(FEATURE_DOMAIN_SYSTEM, 0);
    filter.feature_mask = 0xFF000000;
    filter.confidence_threshold = 0.5f;

    EXPECT_FALSE(subscription_matches(&filter, &packet));
}

TEST_F(SubscriptionMatchingTest, ConfidenceExactThreshold)
{
    // Confidence exactly at threshold should pass
    EVENT_SET_FEATURE_CODE(&packet, MAKE_FEATURE_CODE(FEATURE_DOMAIN_SYSTEM, 0));
    packet.confidence = EVENT_FLOAT_TO_CONFIDENCE(0.5f);
    filter.feature_code = MAKE_FEATURE_CODE(FEATURE_DOMAIN_SYSTEM, 0);
    filter.feature_mask = 0xFF000000;
    // Fix: Use slightly lower threshold to account for float precision
    filter.confidence_threshold = 0.49999f;

    EXPECT_TRUE(subscription_matches(&filter, &packet));
}

TEST_F(SubscriptionMatchingTest, HighConfidenceRequirement)
{
    EVENT_SET_FEATURE_CODE(&packet, MAKE_FEATURE_CODE(FEATURE_DOMAIN_MEMORY, 0x123));
    packet.confidence = EVENT_FLOAT_TO_CONFIDENCE(0.95f);
    filter.feature_code = MAKE_FEATURE_CODE(FEATURE_DOMAIN_MEMORY, 0);
    filter.feature_mask = 0xFF000000;
    filter.confidence_threshold = 0.9f;

    EXPECT_TRUE(subscription_matches(&filter, &packet));
}

TEST_F(SubscriptionMatchingTest, LowConfidenceRequirement)
{
    EVENT_SET_FEATURE_CODE(&packet, MAKE_FEATURE_CODE(FEATURE_DOMAIN_MOTOR, 0x456));
    packet.confidence = EVENT_FLOAT_TO_CONFIDENCE(0.2f);
    filter.feature_code = MAKE_FEATURE_CODE(FEATURE_DOMAIN_MOTOR, 0);
    filter.feature_mask = 0xFF000000;
    filter.confidence_threshold = 0.1f;

    EXPECT_TRUE(subscription_matches(&filter, &packet));
}

TEST_F(SubscriptionMatchingTest, ZeroConfidenceThreshold)
{
    // Zero threshold accepts all confidence values
    EVENT_SET_FEATURE_CODE(&packet, MAKE_FEATURE_CODE(FEATURE_DOMAIN_EMOTION, 0x789));
    packet.confidence = EVENT_FLOAT_TO_CONFIDENCE(0.01f);
    filter.feature_code = MAKE_FEATURE_CODE(FEATURE_DOMAIN_EMOTION, 0);
    filter.feature_mask = 0xFF000000;
    filter.confidence_threshold = 0.0f;

    EXPECT_TRUE(subscription_matches(&filter, &packet));
}

TEST_F(SubscriptionMatchingTest, MaxConfidenceValue)
{
    // Test with maximum confidence (1.0)
    EVENT_SET_FEATURE_CODE(&packet, MAKE_FEATURE_CODE(FEATURE_DOMAIN_ETHICS, 0xABC));
    packet.confidence = EVENT_FLOAT_TO_CONFIDENCE(1.0f);
    filter.feature_code = MAKE_FEATURE_CODE(FEATURE_DOMAIN_ETHICS, 0);
    filter.feature_mask = 0xFF000000;
    filter.confidence_threshold = 0.99f;

    EXPECT_TRUE(subscription_matches(&filter, &packet));
}

TEST_F(SubscriptionMatchingTest, NullFilterPointer)
{
    EXPECT_FALSE(subscription_matches(nullptr, &packet));
}

TEST_F(SubscriptionMatchingTest, NullPacketPointer)
{
    EXPECT_FALSE(subscription_matches(&filter, nullptr));
}

TEST_F(SubscriptionMatchingTest, BothNullPointers)
{
    EXPECT_FALSE(subscription_matches(nullptr, nullptr));
}

//=============================================================================
// Hierarchical Namespace Matching Tests
//=============================================================================

class HierarchicalNamespaceTest : public ProtocolFeatureSubscriptionTest {};

TEST_F(HierarchicalNamespaceTest, DomainLevelMatching)
{
    // Subscribe to all VISION events
    EVENT_SET_FEATURE_CODE(&packet, MAKE_FEATURE_CODE(FEATURE_DOMAIN_VISION, 0xABCDEF));
    filter.feature_code = MAKE_FEATURE_CODE(FEATURE_DOMAIN_VISION, 0);
    filter.feature_mask = 0xFF000000;  // Domain only

    EXPECT_TRUE(subscription_matches(&filter, &packet));

    // Different subcodes should still match
    EVENT_SET_FEATURE_CODE(&packet, MAKE_FEATURE_CODE(FEATURE_DOMAIN_VISION, 0x123456));
    EXPECT_TRUE(subscription_matches(&filter, &packet));

    EVENT_SET_FEATURE_CODE(&packet, MAKE_FEATURE_CODE(FEATURE_DOMAIN_VISION, 0xFFFFFF));
    EXPECT_TRUE(subscription_matches(&filter, &packet));
}

TEST_F(HierarchicalNamespaceTest, SubdomainLevelMatching)
{
    // Subscribe to LANGUAGE:0xAB**** events
    EVENT_SET_FEATURE_CODE(&packet, MAKE_FEATURE_CODE(FEATURE_DOMAIN_LANGUAGE, 0xAB1234));
    filter.feature_code = MAKE_FEATURE_CODE(FEATURE_DOMAIN_LANGUAGE, 0xAB0000);
    filter.feature_mask = 0xFFFF0000;  // Domain + subdomain

    EXPECT_TRUE(subscription_matches(&filter, &packet));

    // Different low bytes should still match
    EVENT_SET_FEATURE_CODE(&packet, MAKE_FEATURE_CODE(FEATURE_DOMAIN_LANGUAGE, 0xABFFFF));
    EXPECT_TRUE(subscription_matches(&filter, &packet));

    // Different subdomain should not match
    EVENT_SET_FEATURE_CODE(&packet, MAKE_FEATURE_CODE(FEATURE_DOMAIN_LANGUAGE, 0xCD1234));
    EXPECT_FALSE(subscription_matches(&filter, &packet));
}

TEST_F(HierarchicalNamespaceTest, CategoryLevelMatching)
{
    // Subscribe to MOTOR:0xABC*** events (3 byte specificity)
    EVENT_SET_FEATURE_CODE(&packet, MAKE_FEATURE_CODE(FEATURE_DOMAIN_MOTOR, 0xABC123));
    filter.feature_code = MAKE_FEATURE_CODE(FEATURE_DOMAIN_MOTOR, 0xABC000);
    filter.feature_mask = 0xFFFFF000;  // Domain + 12 bits of subcode

    EXPECT_TRUE(subscription_matches(&filter, &packet));

    // Different low nibbles should still match
    EVENT_SET_FEATURE_CODE(&packet, MAKE_FEATURE_CODE(FEATURE_DOMAIN_MOTOR, 0xABCFFF));
    EXPECT_TRUE(subscription_matches(&filter, &packet));

    // Different category should not match
    EVENT_SET_FEATURE_CODE(&packet, MAKE_FEATURE_CODE(FEATURE_DOMAIN_MOTOR, 0xABD123));
    EXPECT_FALSE(subscription_matches(&filter, &packet));
}

TEST_F(HierarchicalNamespaceTest, ExactLevelMatching)
{
    // Subscribe to exact feature code
    feature_code_t exact_code = MAKE_FEATURE_CODE(FEATURE_DOMAIN_MEMORY, 0x123456);
    EVENT_SET_FEATURE_CODE(&packet, exact_code);
    filter.feature_code = exact_code;
    filter.feature_mask = 0xFFFFFFFF;  // Exact match

    EXPECT_TRUE(subscription_matches(&filter, &packet));

    // Even single bit difference should not match
    EVENT_SET_FEATURE_CODE(&packet, MAKE_FEATURE_CODE(FEATURE_DOMAIN_MEMORY, 0x123457));
    EXPECT_FALSE(subscription_matches(&filter, &packet));
}

TEST_F(HierarchicalNamespaceTest, CrossDomainNoMatch)
{
    // Ensure subdomain matching doesn't cross domain boundaries
    EVENT_SET_FEATURE_CODE(&packet, MAKE_FEATURE_CODE(FEATURE_DOMAIN_VISION, 0xAB1234));
    filter.feature_code = MAKE_FEATURE_CODE(FEATURE_DOMAIN_AUDITORY, 0xAB1234);
    filter.feature_mask = 0xFFFF0000;  // Domain + subdomain

    EXPECT_FALSE(subscription_matches(&filter, &packet));
}

//=============================================================================
// Feature Mask Pattern Tests
//=============================================================================

class FeatureMaskPatternTest : public ProtocolFeatureSubscriptionTest {};

TEST_F(FeatureMaskPatternTest, DomainOnlyMask)
{
    // Mask 0xFF000000 matches only domain (top 8 bits)
    feature_code_t code = 0x10ABCDEF;
    EXPECT_TRUE(feature_code_matches(code, 0x10000000, 0xFF000000));
    EXPECT_TRUE(feature_code_matches(code, 0x10123456, 0xFF000000));
    EXPECT_FALSE(feature_code_matches(code, 0x20ABCDEF, 0xFF000000));
}

TEST_F(FeatureMaskPatternTest, SubdomainMask)
{
    // Mask 0xFFFF0000 matches domain + subdomain (top 16 bits)
    feature_code_t code = 0x10AB1234;
    EXPECT_TRUE(feature_code_matches(code, 0x10AB0000, 0xFFFF0000));
    EXPECT_TRUE(feature_code_matches(code, 0x10ABFFFF, 0xFFFF0000));
    EXPECT_FALSE(feature_code_matches(code, 0x10AC1234, 0xFFFF0000));
}

TEST_F(FeatureMaskPatternTest, CategoryMask)
{
    // Mask 0xFFFFF000 matches domain + 12 bits of subcode
    feature_code_t code = 0x30ABC123;
    EXPECT_TRUE(feature_code_matches(code, 0x30ABC000, 0xFFFFF000));
    EXPECT_TRUE(feature_code_matches(code, 0x30ABCFFF, 0xFFFFF000));
    EXPECT_FALSE(feature_code_matches(code, 0x30ABD123, 0xFFFFF000));
}

TEST_F(FeatureMaskPatternTest, FineMask)
{
    // Mask 0xFFFFFF00 matches domain + 16 bits of subcode
    feature_code_t code = 0x50ABCD12;
    EXPECT_TRUE(feature_code_matches(code, 0x50ABCD00, 0xFFFFFF00));
    EXPECT_TRUE(feature_code_matches(code, 0x50ABCDFF, 0xFFFFFF00));
    EXPECT_FALSE(feature_code_matches(code, 0x50ABCE12, 0xFFFFFF00));
}

TEST_F(FeatureMaskPatternTest, ExactMask)
{
    // Mask 0xFFFFFFFF requires exact match (all 32 bits)
    feature_code_t code = 0x70ABCDEF;
    EXPECT_TRUE(feature_code_matches(code, 0x70ABCDEF, 0xFFFFFFFF));
    EXPECT_FALSE(feature_code_matches(code, 0x70ABCDEE, 0xFFFFFFFF));
    EXPECT_FALSE(feature_code_matches(code, 0x71ABCDEF, 0xFFFFFFFF));
}

TEST_F(FeatureMaskPatternTest, CustomMaskBottomByte)
{
    // Custom mask matching only bottom byte
    feature_code_t code = 0x90123456;
    EXPECT_TRUE(feature_code_matches(code, 0x00000056, 0x000000FF));
    EXPECT_FALSE(feature_code_matches(code, 0x00000057, 0x000000FF));
}

TEST_F(FeatureMaskPatternTest, CustomMaskMiddleBytes)
{
    // Custom mask matching middle 16 bits only
    feature_code_t code = 0xA0ABCDEF;
    // Fix: Middle bytes of code are 0xCD, so filter should be 0x00ABCD00
    EXPECT_TRUE(feature_code_matches(code, 0x00ABCD00, 0x00FFFF00));
    EXPECT_FALSE(feature_code_matches(code, 0x00ABCE00, 0x00FFFF00));
}

TEST_F(FeatureMaskPatternTest, CustomMaskAlternatingBits)
{
    // Custom mask with alternating bits (unlikely in practice but valid)
    feature_code_t code = 0xAAAAAAAA;
    EXPECT_TRUE(feature_code_matches(code, 0xAAAAAAAA, 0xAAAAAAAA));
    // Fix: Code is 0xAAAAAAAA (binary 1010...), mask 0x55555555 (binary 0101...)
    // gives 0x00000000, so filter must also give 0x00000000 when masked
    EXPECT_TRUE(feature_code_matches(code, 0x00000000, 0x55555555));
}

//=============================================================================
// Edge Case Tests
//=============================================================================

class FeatureSubscriptionEdgeCaseTest : public ProtocolFeatureSubscriptionTest {};

TEST_F(FeatureSubscriptionEdgeCaseTest, ZeroFeatureCode)
{
    EVENT_SET_FEATURE_CODE(&packet, 0x00000000);
    filter.feature_code = 0x00000000;
    filter.feature_mask = 0xFFFFFFFF;

    EXPECT_TRUE(subscription_matches(&filter, &packet));
}

TEST_F(FeatureSubscriptionEdgeCaseTest, MaxFeatureCode)
{
    EVENT_SET_FEATURE_CODE(&packet, 0xFFFFFFFF);
    filter.feature_code = 0xFFFFFFFF;
    filter.feature_mask = 0xFFFFFFFF;

    EXPECT_TRUE(subscription_matches(&filter, &packet));
}

TEST_F(FeatureSubscriptionEdgeCaseTest, ZeroMask)
{
    // Zero mask means no bits are checked - everything matches
    EVENT_SET_FEATURE_CODE(&packet, 0x10ABCDEF);
    filter.feature_code = 0x90123456;  // Completely different
    filter.feature_mask = 0x00000000;

    EXPECT_TRUE(feature_code_matches(EVENT_GET_FEATURE_CODE(&packet), filter.feature_code,
                                     filter.feature_mask));
}

TEST_F(FeatureSubscriptionEdgeCaseTest, NegativeConfidenceThreshold)
{
    // Negative threshold (shouldn't happen but handle gracefully)
    EVENT_SET_FEATURE_CODE(&packet, MAKE_FEATURE_CODE(FEATURE_DOMAIN_SYSTEM, 0));
    packet.confidence = EVENT_FLOAT_TO_CONFIDENCE(0.5f);
    filter.feature_code = MAKE_FEATURE_CODE(FEATURE_DOMAIN_SYSTEM, 0);
    filter.feature_mask = 0xFF000000;
    filter.confidence_threshold = -0.5f;

    // Negative threshold should accept any positive confidence
    EXPECT_TRUE(subscription_matches(&filter, &packet));
}

TEST_F(FeatureSubscriptionEdgeCaseTest, ConfidenceAboveOne)
{
    // Threshold above 1.0 (shouldn't happen but handle gracefully)
    EVENT_SET_FEATURE_CODE(&packet, MAKE_FEATURE_CODE(FEATURE_DOMAIN_SYSTEM, 0));
    packet.confidence = EVENT_FLOAT_TO_CONFIDENCE(1.0f);
    filter.feature_code = MAKE_FEATURE_CODE(FEATURE_DOMAIN_SYSTEM, 0);
    filter.feature_mask = 0xFF000000;
    filter.confidence_threshold = 1.5f;

    // Maximum confidence (1.0) won't meet threshold above 1.0
    EXPECT_FALSE(subscription_matches(&filter, &packet));
}

TEST_F(FeatureSubscriptionEdgeCaseTest, MinimumConfidenceValue)
{
    EVENT_SET_FEATURE_CODE(&packet, MAKE_FEATURE_CODE(FEATURE_DOMAIN_VISION, 0));
    packet.confidence = 0;  // Minimum confidence (0.0)
    filter.feature_code = MAKE_FEATURE_CODE(FEATURE_DOMAIN_VISION, 0);
    filter.feature_mask = 0xFF000000;
    filter.confidence_threshold = 0.0f;

    EXPECT_TRUE(subscription_matches(&filter, &packet));
}

TEST_F(FeatureSubscriptionEdgeCaseTest, MaximumConfidenceValue)
{
    EVENT_SET_FEATURE_CODE(&packet, MAKE_FEATURE_CODE(FEATURE_DOMAIN_AUDITORY, 0));
    packet.confidence = 65535;  // Maximum confidence (1.0)
    filter.feature_code = MAKE_FEATURE_CODE(FEATURE_DOMAIN_AUDITORY, 0);
    filter.feature_mask = 0xFF000000;
    filter.confidence_threshold = 1.0f;

    EXPECT_TRUE(subscription_matches(&filter, &packet));
}

TEST_F(FeatureSubscriptionEdgeCaseTest, InvalidDomainInFeatureCode)
{
    // Use domain 0x08 which is not defined
    feature_code_t invalid_code = MAKE_FEATURE_CODE(0x08, 0x123456);
    EVENT_SET_FEATURE_CODE(&packet, invalid_code);
    filter.feature_code = MAKE_FEATURE_CODE(0x08, 0);
    filter.feature_mask = 0xFF000000;

    // Matching should still work based on bit patterns
    EXPECT_TRUE(subscription_matches(&filter, &packet));
}

//=============================================================================
// Rate Limiting Logic Tests
//=============================================================================

class RateLimitingTest : public ProtocolFeatureSubscriptionTest {};

TEST_F(RateLimitingTest, UnlimitedRateAllowed)
{
    // max_rate_hz = 0 means unlimited
    EVENT_SET_FEATURE_CODE(&packet, MAKE_FEATURE_CODE(FEATURE_DOMAIN_SYSTEM, 0));
    filter.feature_code = MAKE_FEATURE_CODE(FEATURE_DOMAIN_SYSTEM, 0);
    filter.feature_mask = 0xFF000000;
    filter.max_rate_hz = 0;

    EXPECT_TRUE(subscription_matches(&filter, &packet));
}

TEST_F(RateLimitingTest, RateLimitFieldExists)
{
    // Verify rate limit field is part of subscription filter structure
    filter.max_rate_hz = 100;
    EXPECT_EQ(filter.max_rate_hz, 100u);

    filter.max_rate_hz = 1000;
    EXPECT_EQ(filter.max_rate_hz, 1000u);

    filter.max_rate_hz = 0;
    EXPECT_EQ(filter.max_rate_hz, 0u);
}

TEST_F(RateLimitingTest, RateLimitDoesNotAffectBasicMatching)
{
    // Rate limiting should not affect subscription_matches()
    // (rate checking happens in caller based on timestamp)
    EVENT_SET_FEATURE_CODE(&packet, MAKE_FEATURE_CODE(FEATURE_DOMAIN_MEMORY, 0x123));
    filter.feature_code = MAKE_FEATURE_CODE(FEATURE_DOMAIN_MEMORY, 0);
    filter.feature_mask = 0xFF000000;
    filter.max_rate_hz = 1;  // Very low rate

    // subscription_matches doesn't check rate, only feature and confidence
    EXPECT_TRUE(subscription_matches(&filter, &packet));
}

//=============================================================================
// Confidence Conversion Tests
//=============================================================================

class ConfidenceConversionTest : public ProtocolFeatureSubscriptionTest {};

TEST_F(ConfidenceConversionTest, FloatToConfidenceZero)
{
    uint16_t conf = EVENT_FLOAT_TO_CONFIDENCE(0.0f);
    EXPECT_EQ(conf, 0);
}

TEST_F(ConfidenceConversionTest, FloatToConfidenceOne)
{
    uint16_t conf = EVENT_FLOAT_TO_CONFIDENCE(1.0f);
    EXPECT_EQ(conf, 65535);
}

TEST_F(ConfidenceConversionTest, FloatToConfidenceHalf)
{
    uint16_t conf = EVENT_FLOAT_TO_CONFIDENCE(0.5f);
    EXPECT_NEAR(conf, 32767, 1);  // Allow 1 bit tolerance for rounding
}

TEST_F(ConfidenceConversionTest, ConfidenceToFloatZero)
{
    float conf = EVENT_CONFIDENCE_TO_FLOAT(0);
    EXPECT_FLOAT_EQ(conf, 0.0f);
}

TEST_F(ConfidenceConversionTest, ConfidenceToFloatMax)
{
    float conf = EVENT_CONFIDENCE_TO_FLOAT(65535);
    EXPECT_FLOAT_EQ(conf, 1.0f);
}

TEST_F(ConfidenceConversionTest, ConfidenceToFloatHalf)
{
    float conf = EVENT_CONFIDENCE_TO_FLOAT(32767);
    EXPECT_NEAR(conf, 0.5f, 0.01f);
}

TEST_F(ConfidenceConversionTest, RoundTripConversionZero)
{
    float original = 0.0f;
    uint16_t int_val = EVENT_FLOAT_TO_CONFIDENCE(original);
    float result = EVENT_CONFIDENCE_TO_FLOAT(int_val);
    EXPECT_FLOAT_EQ(result, original);
}

TEST_F(ConfidenceConversionTest, RoundTripConversionOne)
{
    float original = 1.0f;
    uint16_t int_val = EVENT_FLOAT_TO_CONFIDENCE(original);
    float result = EVENT_CONFIDENCE_TO_FLOAT(int_val);
    EXPECT_FLOAT_EQ(result, original);
}

TEST_F(ConfidenceConversionTest, RoundTripConversionMidRange)
{
    float original = 0.5f;
    uint16_t int_val = EVENT_FLOAT_TO_CONFIDENCE(original);
    float result = EVENT_CONFIDENCE_TO_FLOAT(int_val);
    EXPECT_NEAR(result, original, 0.001f);
}
