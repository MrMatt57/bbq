#pragma once

#include <stdint.h>

#define GRAPH_HISTORY_SIZE 240

// A single condensable graph data slot
struct GraphSlot {
    float pit;
    float meat1;
    float meat2;
    float setpoint;
    bool pitValid;
    bool meat1Valid;
    bool meat2Valid;
};

// Adaptive-condensing graph history buffer.
// Stores up to 240 slots. When full, merges all 240 into 120 by pairwise
// averaging, then continues appending from slot 120. A 12-hour cook at 5s
// intervals triggers ~5-6 merges; oldest points gradually represent wider
// time spans while recent data stays detailed.
//
// Pure C++ — no LVGL or Arduino dependencies. Fully testable on native.
class GraphHistory {
public:
    GraphHistory();

    // Append a data point. Disconnected probes are marked invalid.
    // When the buffer is full, condenses 240 -> 120 before appending.
    void addPoint(float pit, float meat1, float meat2, float setpoint,
                  bool pitDisc, bool meat1Disc, bool meat2Disc);

    // Clear all stored data
    void clear();

    // Number of valid slots currently stored
    uint16_t getCount() const { return _count; }

    // Access a slot by index (0 = oldest)
    const GraphSlot& getSlot(uint16_t index) const;

private:
    GraphSlot _buffer[GRAPH_HISTORY_SIZE];
    uint16_t _count;

    // Merge the full buffer into half by pairwise averaging
    void condense();

    // Average two values respecting validity flags
    static float mergeValues(float a, bool aValid, float b, bool bValid, bool& outValid);
};
