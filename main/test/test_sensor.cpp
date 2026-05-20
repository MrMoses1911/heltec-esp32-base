#include "unity.h"
#include "sensors/q5x_pfm.h"

void test_sensor_constants(void) {
    TEST_ASSERT_EQUAL_FLOAT(100.0f, Q5X_PFM::kMinFreqHz);
    TEST_ASSERT_EQUAL_FLOAT(600.0f, Q5X_PFM::kMaxFreqHz);
    TEST_ASSERT_EQUAL_FLOAT(50.0f, Q5X_PFM::kMinDistMm);
    TEST_ASSERT_EQUAL_FLOAT(5000.0f, Q5X_PFM::kMaxDistMm);
}

void test_sensor_formula(void) {
    float slope = (Q5X_PFM::kMaxDistMm - Q5X_PFM::kMinDistMm) /
                  (Q5X_PFM::kMaxFreqHz - Q5X_PFM::kMinFreqHz);
    TEST_ASSERT_EQUAL_FLOAT(9.9f, slope);

    float d1 = Q5X_PFM::kMinDistMm + (Q5X_PFM::kMinFreqHz - Q5X_PFM::kMinFreqHz) * slope;
    TEST_ASSERT_EQUAL_FLOAT(50.0f, d1);

    float d2 = Q5X_PFM::kMinDistMm + (Q5X_PFM::kMaxFreqHz - Q5X_PFM::kMinFreqHz) * slope;
    TEST_ASSERT_EQUAL_FLOAT(5000.0f, d2);
}

void test_sensor_period_limits(void) {
    float minPeriod = 1000000.0f / Q5X_PFM::kMaxFreqHz;
    float maxPeriod = 1000000.0f / Q5X_PFM::kMinFreqHz;
    TEST_ASSERT_TRUE(minPeriod > 1660.0f && minPeriod < 1670.0f);
    TEST_ASSERT_TRUE(maxPeriod > 9990.0f && maxPeriod < 10010.0f);
}
