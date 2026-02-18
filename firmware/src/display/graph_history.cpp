#include "graph_history.h"

GraphHistory::GraphHistory() : _count(0) {}

void GraphHistory::addPoint(float pit, float meat1, float meat2, float setpoint,
                             bool pitDisc, bool meat1Disc, bool meat2Disc) {
    if (_count >= GRAPH_HISTORY_SIZE) {
        condense();
    }

    GraphSlot& slot = _buffer[_count];
    slot.pit = pit;
    slot.meat1 = meat1;
    slot.meat2 = meat2;
    slot.setpoint = setpoint;
    slot.pitValid = !pitDisc;
    slot.meat1Valid = !meat1Disc;
    slot.meat2Valid = !meat2Disc;
    _count++;
}

void GraphHistory::clear() {
    _count = 0;
}

const GraphSlot& GraphHistory::getSlot(uint16_t index) const {
    static const GraphSlot empty = {0, 0, 0, 0, false, false, false};
    if (index >= _count) return empty;
    return _buffer[index];
}

float GraphHistory::mergeValues(float a, bool aValid, float b, bool bValid, bool& outValid) {
    if (aValid && bValid) {
        outValid = true;
        return (a + b) * 0.5f;
    } else if (aValid) {
        outValid = true;
        return a;
    } else if (bValid) {
        outValid = true;
        return b;
    } else {
        outValid = false;
        return 0.0f;
    }
}

void GraphHistory::condense() {
    uint16_t dst = 0;
    for (uint16_t i = 0; i < _count; i += 2) {
        if (i + 1 < _count) {
            const GraphSlot& a = _buffer[i];
            const GraphSlot& b = _buffer[i + 1];
            GraphSlot& out = _buffer[dst];

            out.pit = mergeValues(a.pit, a.pitValid, b.pit, b.pitValid, out.pitValid);
            out.meat1 = mergeValues(a.meat1, a.meat1Valid, b.meat1, b.meat1Valid, out.meat1Valid);
            out.meat2 = mergeValues(a.meat2, a.meat2Valid, b.meat2, b.meat2Valid, out.meat2Valid);
            out.setpoint = (a.setpoint + b.setpoint) * 0.5f;
        } else {
            _buffer[dst] = _buffer[i];
        }
        dst++;
    }
    _count = dst;
}
