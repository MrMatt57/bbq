/**
 * test_session.cpp
 *
 * Tests for CookSession logic on the native platform.
 *
 * LittleFS operations (flush, loadFromFlash) are guarded by #ifndef NATIVE_BUILD,
 * so they are no-ops on native. The begin() method, update() method, and flush()
 * are also guarded. We test the pure in-memory logic:
 *   - DataPoint encoding (temps stored as int16 * 10)
 *   - Circular buffer (addPoint, getPoint, wrapping)
 *   - Point count tracking
 *   - CSV generation format
 *   - Session active state management
 *
 * The String class is used by toCSV() and toJSON(). On native builds with
 * PlatformIO, the Arduino String class is not available. We provide a minimal
 * String implementation to allow compilation.
 */

#include <unity.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// --------------------------------------------------------------------------
// Minimal Arduino String stub for native build
// --------------------------------------------------------------------------
#ifdef NATIVE_BUILD

#include <string>

class String {
public:
    String() : _data() {}
    String(const char* s) : _data(s ? s : "") {}
    String(const String& other) : _data(other._data) {}

    String& operator=(const String& other) {
        _data = other._data;
        return *this;
    }

    String& operator+=(const char* s) {
        if (s) _data += s;
        return *this;
    }

    String& operator+=(const String& other) {
        _data += other._data;
        return *this;
    }

    void reserve(size_t size) {
        _data.reserve(size);
    }

    const char* c_str() const {
        return _data.c_str();
    }

    size_t length() const {
        return _data.length();
    }

    int indexOf(const char* s) const {
        size_t pos = _data.find(s);
        if (pos == std::string::npos) return -1;
        return (int)pos;
    }

    int indexOf(char c) const {
        size_t pos = _data.find(c);
        if (pos == std::string::npos) return -1;
        return (int)pos;
    }

private:
    std::string _data;
};

#endif  // NATIVE_BUILD

// Now include the module under test
#include "cook_session.h"
#include "cook_session.cpp"

// --------------------------------------------------------------------------
// setUp / tearDown
// --------------------------------------------------------------------------

static CookSession* session;

void setUp(void) {
    session = new CookSession();
    // Note: we do NOT call begin() because it tries to loadFromFlash()
    // which is a no-op on native but also not needed
}

void tearDown(void) {
    delete session;
    session = nullptr;
}

// --------------------------------------------------------------------------
// Helper: create a DataPoint with given values
// --------------------------------------------------------------------------

static DataPoint makePoint(uint32_t ts, float pitF, float meat1F, float meat2F,
                            uint8_t fan, uint8_t damper, uint8_t flags) {
    DataPoint dp;
    dp.timestamp = ts;
    dp.pitTemp   = (int16_t)(pitF * 10.0f);
    dp.meat1Temp = (int16_t)(meat1F * 10.0f);
    dp.meat2Temp = (int16_t)(meat2F * 10.0f);
    dp.fanPct    = fan;
    dp.damperPct = damper;
    dp.flags     = flags;
    return dp;
}

// --------------------------------------------------------------------------
// Tests: DataPoint encoding
// --------------------------------------------------------------------------

void test_datapoint_encoding_positive_temp(void) {
    // 225.5F should be stored as 2255
    DataPoint dp = makePoint(1000, 225.5f, 0.0f, 0.0f, 0, 0, 0);
    TEST_ASSERT_EQUAL_INT16(2255, dp.pitTemp);
}

void test_datapoint_encoding_zero_temp(void) {
    DataPoint dp = makePoint(1000, 0.0f, 0.0f, 0.0f, 0, 0, 0);
    TEST_ASSERT_EQUAL_INT16(0, dp.pitTemp);
}

void test_datapoint_encoding_negative_temp(void) {
    // -10.5F should be stored as -105
    DataPoint dp = makePoint(1000, -10.5f, 0.0f, 0.0f, 0, 0, 0);
    TEST_ASSERT_EQUAL_INT16(-105, dp.pitTemp);
}

void test_datapoint_encoding_all_fields(void) {
    DataPoint dp = makePoint(1700000000, 250.0f, 165.3f, 0.0f, 45, 60, DP_FLAG_LID_OPEN);
    TEST_ASSERT_EQUAL_UINT32(1700000000, dp.timestamp);
    TEST_ASSERT_EQUAL_INT16(2500, dp.pitTemp);
    TEST_ASSERT_EQUAL_INT16(1653, dp.meat1Temp);
    TEST_ASSERT_EQUAL_INT16(0, dp.meat2Temp);
    TEST_ASSERT_EQUAL_UINT8(45, dp.fanPct);
    TEST_ASSERT_EQUAL_UINT8(60, dp.damperPct);
    TEST_ASSERT_EQUAL_UINT8(DP_FLAG_LID_OPEN, dp.flags);
}

void test_datapoint_flags_bitmask(void) {
    uint8_t flags = DP_FLAG_LID_OPEN | DP_FLAG_ALARM_PIT | DP_FLAG_MEAT1_DISC;
    TEST_ASSERT_EQUAL_UINT8(0x01 | 0x02 | 0x40, flags);
    TEST_ASSERT_TRUE(flags & DP_FLAG_LID_OPEN);
    TEST_ASSERT_TRUE(flags & DP_FLAG_ALARM_PIT);
    TEST_ASSERT_TRUE(flags & DP_FLAG_MEAT1_DISC);
    TEST_ASSERT_FALSE(flags & DP_FLAG_ALARM_MEAT1);
}

// --------------------------------------------------------------------------
// Tests: Initial state
// --------------------------------------------------------------------------

void test_initial_point_count_zero(void) {
    TEST_ASSERT_EQUAL_UINT32(0, session->getPointCount());
}

void test_initial_total_point_count_zero(void) {
    TEST_ASSERT_EQUAL_UINT32(0, session->getTotalPointCount());
}

void test_initial_not_active(void) {
    TEST_ASSERT_FALSE(session->isActive());
}

void test_initial_start_time_zero(void) {
    TEST_ASSERT_EQUAL_UINT32(0, session->getStartTime());
}

// --------------------------------------------------------------------------
// Tests: addPoint and getPoint
// --------------------------------------------------------------------------

void test_add_single_point(void) {
    DataPoint dp = makePoint(1000, 225.0f, 0.0f, 0.0f, 50, 30, 0);
    session->addPoint(dp);

    TEST_ASSERT_EQUAL_UINT32(1, session->getPointCount());
    TEST_ASSERT_EQUAL_UINT32(1, session->getTotalPointCount());

    const DataPoint* retrieved = session->getPoint(0);
    TEST_ASSERT_NOT_NULL(retrieved);
    TEST_ASSERT_EQUAL_UINT32(1000, retrieved->timestamp);
    TEST_ASSERT_EQUAL_INT16(2250, retrieved->pitTemp);
    TEST_ASSERT_EQUAL_UINT8(50, retrieved->fanPct);
}

void test_add_multiple_points(void) {
    for (uint32_t i = 0; i < 10; i++) {
        DataPoint dp = makePoint(1000 + i * 5, 200.0f + (float)i, 0.0f, 0.0f, 0, 0, 0);
        session->addPoint(dp);
    }

    TEST_ASSERT_EQUAL_UINT32(10, session->getPointCount());

    // Verify first point
    const DataPoint* first = session->getPoint(0);
    TEST_ASSERT_NOT_NULL(first);
    TEST_ASSERT_EQUAL_UINT32(1000, first->timestamp);
    TEST_ASSERT_EQUAL_INT16(2000, first->pitTemp);

    // Verify last point
    const DataPoint* last = session->getPoint(9);
    TEST_ASSERT_NOT_NULL(last);
    TEST_ASSERT_EQUAL_UINT32(1045, last->timestamp);
    TEST_ASSERT_EQUAL_INT16(2090, last->pitTemp);
}

void test_getPoint_out_of_range_returns_null(void) {
    DataPoint dp = makePoint(1000, 200.0f, 0.0f, 0.0f, 0, 0, 0);
    session->addPoint(dp);

    const DataPoint* retrieved = session->getPoint(1);  // Only 1 point, index 1 is out of range
    TEST_ASSERT_NULL(retrieved);
}

void test_getPoint_on_empty_returns_null(void) {
    const DataPoint* retrieved = session->getPoint(0);
    TEST_ASSERT_NULL(retrieved);
}

// --------------------------------------------------------------------------
// Tests: Circular buffer wrapping
// --------------------------------------------------------------------------

void test_circular_buffer_wrapping(void) {
    // Fill the buffer completely and then some more
    uint32_t totalToAdd = SESSION_BUFFER_SIZE + 50;

    for (uint32_t i = 0; i < totalToAdd; i++) {
        DataPoint dp = makePoint(1000 + i, 200.0f, 0.0f, 0.0f, 0, 0, 0);
        dp.pitTemp = (int16_t)(i & 0x7FFF);  // Use index as marker
        session->addPoint(dp);
    }

    // Count should be capped at buffer size
    TEST_ASSERT_EQUAL_UINT32(SESSION_BUFFER_SIZE, session->getPointCount());

    // Total points includes all added
    TEST_ASSERT_EQUAL_UINT32(totalToAdd, session->getTotalPointCount());

    // The oldest point in the buffer should be the one added at index 50
    // (the first 50 were overwritten)
    const DataPoint* oldest = session->getPoint(0);
    TEST_ASSERT_NOT_NULL(oldest);
    TEST_ASSERT_EQUAL_UINT32(1050, oldest->timestamp);

    // The newest point should be the last one added
    const DataPoint* newest = session->getPoint(SESSION_BUFFER_SIZE - 1);
    TEST_ASSERT_NOT_NULL(newest);
    TEST_ASSERT_EQUAL_UINT32(1000 + totalToAdd - 1, newest->timestamp);
}

void test_circular_buffer_exact_fill(void) {
    // Fill exactly to capacity
    for (uint32_t i = 0; i < SESSION_BUFFER_SIZE; i++) {
        DataPoint dp = makePoint(i, 200.0f, 0.0f, 0.0f, 0, 0, 0);
        session->addPoint(dp);
    }

    TEST_ASSERT_EQUAL_UINT32(SESSION_BUFFER_SIZE, session->getPointCount());
    TEST_ASSERT_EQUAL_UINT32(SESSION_BUFFER_SIZE, session->getTotalPointCount());

    // First point should be timestamp 0
    const DataPoint* first = session->getPoint(0);
    TEST_ASSERT_NOT_NULL(first);
    TEST_ASSERT_EQUAL_UINT32(0, first->timestamp);

    // Last point
    const DataPoint* last = session->getPoint(SESSION_BUFFER_SIZE - 1);
    TEST_ASSERT_NOT_NULL(last);
    TEST_ASSERT_EQUAL_UINT32(SESSION_BUFFER_SIZE - 1, last->timestamp);
}

// --------------------------------------------------------------------------
// Tests: CSV generation
// --------------------------------------------------------------------------

void test_csv_header(void) {
    // Even with no data, toCSV should produce a header line
    String csv = session->toCSV();
    TEST_ASSERT_TRUE(csv.indexOf("timestamp,pit,meat1,meat2,fan,damper,flags") >= 0);
}

void test_csv_single_point(void) {
    DataPoint dp = makePoint(1700000000, 225.5f, 165.0f, 0.0f, 45, 60, 0x01);
    session->addPoint(dp);

    String csv = session->toCSV();

    // Check header exists
    TEST_ASSERT_TRUE(csv.indexOf("timestamp,pit,meat1,meat2,fan,damper,flags") >= 0);

    // Check the data line. The format is:
    // timestamp, pit/10, meat1/10, meat2/10, fan, damper, flags
    // pitTemp=2255 -> 225.5, meat1Temp=1650 -> 165.0, meat2Temp=0 -> 0.0
    TEST_ASSERT_TRUE(csv.indexOf("1700000000") >= 0);
    TEST_ASSERT_TRUE(csv.indexOf("225.5") >= 0);
    TEST_ASSERT_TRUE(csv.indexOf("165.0") >= 0);
    TEST_ASSERT_TRUE(csv.indexOf("45") >= 0);
    TEST_ASSERT_TRUE(csv.indexOf("60") >= 0);
}

void test_csv_multiple_points(void) {
    DataPoint dp1 = makePoint(1000, 200.0f, 100.0f, 50.0f, 10, 20, 0);
    DataPoint dp2 = makePoint(1005, 201.0f, 101.0f, 51.0f, 11, 21, 0);
    session->addPoint(dp1);
    session->addPoint(dp2);

    String csv = session->toCSV();

    // Both timestamps should appear
    TEST_ASSERT_TRUE(csv.indexOf("1000") >= 0);
    TEST_ASSERT_TRUE(csv.indexOf("1005") >= 0);
}

// --------------------------------------------------------------------------
// Tests: Session active state management
// --------------------------------------------------------------------------

void test_startSession_sets_active(void) {
    session->startSession();
    TEST_ASSERT_TRUE(session->isActive());
}

void test_endSession_clears_active(void) {
    session->startSession();
    session->endSession();
    TEST_ASSERT_FALSE(session->isActive());
}

void test_startSession_clears_previous_data(void) {
    // Add some points
    DataPoint dp = makePoint(1000, 200.0f, 0.0f, 0.0f, 0, 0, 0);
    session->addPoint(dp);
    session->addPoint(dp);
    TEST_ASSERT_EQUAL_UINT32(2, session->getPointCount());

    // Start a new session (calls clear internally)
    session->startSession();
    TEST_ASSERT_EQUAL_UINT32(0, session->getPointCount());
    TEST_ASSERT_EQUAL_UINT32(0, session->getTotalPointCount());
}

void test_clear_resets_everything(void) {
    DataPoint dp = makePoint(1000, 200.0f, 0.0f, 0.0f, 0, 0, 0);
    session->addPoint(dp);
    session->addPoint(dp);
    session->addPoint(dp);

    session->clear();
    TEST_ASSERT_EQUAL_UINT32(0, session->getPointCount());
    TEST_ASSERT_EQUAL_UINT32(0, session->getTotalPointCount());
    TEST_ASSERT_FALSE(session->isActive());
    TEST_ASSERT_EQUAL_UINT32(0, session->getStartTime());
}

// --------------------------------------------------------------------------
// Tests: loadFromFlash returns false on native
// --------------------------------------------------------------------------

void test_loadFromFlash_returns_false_on_native(void) {
    TEST_ASSERT_FALSE(session->loadFromFlash());
}

// --------------------------------------------------------------------------
// Tests: getElapsedSec returns 0 on native
// --------------------------------------------------------------------------

void test_getElapsedSec_zero_on_native(void) {
    session->startSession();
    TEST_ASSERT_EQUAL_UINT32(0, session->getElapsedSec());
}

// --------------------------------------------------------------------------
// Tests: setDataSources (just verify it doesn't crash)
// --------------------------------------------------------------------------

static float stubPitTemp()   { return 225.0f; }
static float stubMeat1Temp() { return 165.0f; }
static float stubMeat2Temp() { return 0.0f; }
static uint8_t stubFanPct()  { return 50; }
static uint8_t stubDamper()  { return 30; }
static uint8_t stubFlags()   { return 0; }

void test_setDataSources_no_crash(void) {
    session->setDataSources(stubPitTemp, stubMeat1Temp, stubMeat2Temp,
                             stubFanPct, stubDamper, stubFlags);
    // Just verifying no crash
    TEST_ASSERT_TRUE(true);
}

// --------------------------------------------------------------------------
// Tests: DataPoint struct size
// --------------------------------------------------------------------------

void test_datapoint_struct_size(void) {
    // DataPoint should be compact. It has:
    // uint32_t (4) + int16_t (2) + int16_t (2) + int16_t (2) +
    // uint8_t (1) + uint8_t (1) + uint8_t (1) = 13 bytes
    // But due to alignment/packing, it might be slightly larger.
    // We just verify it's reasonably small (<=16 bytes with padding).
    TEST_ASSERT_TRUE(sizeof(DataPoint) <= 16);
}

// --------------------------------------------------------------------------
// Tests: JSON generation
// --------------------------------------------------------------------------

void test_json_empty_is_array(void) {
    String json = session->toJSON();
    TEST_ASSERT_TRUE(json.indexOf("[") >= 0);
    TEST_ASSERT_TRUE(json.indexOf("]") >= 0);
}

void test_json_single_point(void) {
    DataPoint dp = makePoint(1700000000, 225.5f, 0.0f, 0.0f, 50, 30, 0);
    session->addPoint(dp);

    String json = session->toJSON();
    TEST_ASSERT_TRUE(json.indexOf("\"ts\":1700000000") >= 0);
    TEST_ASSERT_TRUE(json.indexOf("\"pit\":225.5") >= 0);
    TEST_ASSERT_TRUE(json.indexOf("\"fan\":50") >= 0);
}

// --------------------------------------------------------------------------
// Main
// --------------------------------------------------------------------------

int main(int argc, char** argv) {
    UNITY_BEGIN();

    // DataPoint encoding
    RUN_TEST(test_datapoint_encoding_positive_temp);
    RUN_TEST(test_datapoint_encoding_zero_temp);
    RUN_TEST(test_datapoint_encoding_negative_temp);
    RUN_TEST(test_datapoint_encoding_all_fields);
    RUN_TEST(test_datapoint_flags_bitmask);

    // Initial state
    RUN_TEST(test_initial_point_count_zero);
    RUN_TEST(test_initial_total_point_count_zero);
    RUN_TEST(test_initial_not_active);
    RUN_TEST(test_initial_start_time_zero);

    // addPoint / getPoint
    RUN_TEST(test_add_single_point);
    RUN_TEST(test_add_multiple_points);
    RUN_TEST(test_getPoint_out_of_range_returns_null);
    RUN_TEST(test_getPoint_on_empty_returns_null);

    // Circular buffer
    RUN_TEST(test_circular_buffer_wrapping);
    RUN_TEST(test_circular_buffer_exact_fill);

    // CSV
    RUN_TEST(test_csv_header);
    RUN_TEST(test_csv_single_point);
    RUN_TEST(test_csv_multiple_points);

    // Session state
    RUN_TEST(test_startSession_sets_active);
    RUN_TEST(test_endSession_clears_active);
    RUN_TEST(test_startSession_clears_previous_data);
    RUN_TEST(test_clear_resets_everything);

    // Native-specific behavior
    RUN_TEST(test_loadFromFlash_returns_false_on_native);
    RUN_TEST(test_getElapsedSec_zero_on_native);

    // Data sources
    RUN_TEST(test_setDataSources_no_crash);

    // Struct size
    RUN_TEST(test_datapoint_struct_size);

    // JSON
    RUN_TEST(test_json_empty_is_array);
    RUN_TEST(test_json_single_point);

    return UNITY_END();
}
