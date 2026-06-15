#pragma once

#include <cmath>
#include <memory>

namespace value {

// Rango numérico [start, end) — producto de range(start, end).
struct RangeValue {
    double start = 0.0;
    double end = 0.0;

    RangeValue() = default;
    RangeValue(double start_value, double end_value) : start(start_value), end(end_value) {}

    std::shared_ptr<RangeIterator> beginIterator() const;
};

// Iterador con protocolo next/current (para for-in).
struct RangeIterator {
    std::shared_ptr<RangeValue> range;
    double current = 0.0;
    bool started = false;
    bool finished = false;

    explicit RangeIterator(std::shared_ptr<RangeValue> range_value)
        : range(std::move(range_value)) {}

    bool next();
    double currentValue() const { return current; }
};

inline std::shared_ptr<RangeIterator> RangeValue::beginIterator() const {
    auto copy = std::make_shared<RangeValue>(start, end);
    return std::make_shared<RangeIterator>(std::move(copy));
}

inline bool RangeIterator::next() {
    if (!range || finished) {
        return false;
    }
    if (!started) {
        started = true;
        current = range->start;
    } else {
        current += 1.0;
    }
    if (current >= range->end) {
        finished = true;
        return false;
    }
    return true;
}

}  // namespace value
