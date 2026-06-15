#pragma once

#include <memory>
#include <ostream>
#include <sstream>
#include <string>
#include <variant>

#include "iterable.hpp"
#include "instance.hpp"

namespace value {

using ValueStorage = std::variant<std::monostate, double, std::string, bool,
                                  std::shared_ptr<RangeValue>, std::shared_ptr<RangeIterator>,
                                  std::shared_ptr<Instance>>;

class Value {
public:
    Value() : storage_(0.0) {}
    static Value null() { return Value(std::monostate{}); }
    explicit Value(double d) : storage_(d) {}
    explicit Value(const std::string& s) : storage_(s) {}
    explicit Value(bool b) : storage_(b) {}
    explicit Value(std::shared_ptr<RangeValue> rv) : storage_(std::move(rv)) {}
    explicit Value(std::shared_ptr<RangeIterator> it) : storage_(std::move(it)) {}
    explicit Value(std::shared_ptr<Instance> inst) : storage_(std::move(inst)) {}

    bool isNull() const { return std::holds_alternative<std::monostate>(storage_); }
    bool isNumber() const { return std::holds_alternative<double>(storage_); }
    bool isString() const { return std::holds_alternative<std::string>(storage_); }
    bool isBool() const { return std::holds_alternative<bool>(storage_); }
    bool isRange() const { return std::holds_alternative<std::shared_ptr<RangeValue>>(storage_); }
    bool isRangeIterator() const {
        return std::holds_alternative<std::shared_ptr<RangeIterator>>(storage_);
    }
    bool isInstance() const { return std::holds_alternative<std::shared_ptr<Instance>>(storage_); }

    double asNumber() const { return std::get<double>(storage_); }
    const std::string& asString() const { return std::get<std::string>(storage_); }
    bool asBool() const { return std::get<bool>(storage_); }
    std::shared_ptr<RangeValue> asRange() const { return std::get<std::shared_ptr<RangeValue>>(storage_); }
    std::shared_ptr<RangeIterator> asRangeIterator() const {
        return std::get<std::shared_ptr<RangeIterator>>(storage_);
    }
    std::shared_ptr<Instance> asInstance() const { return std::get<std::shared_ptr<Instance>>(storage_); }

    std::string toString() const;
    std::string getTypeName() const;

private:
    explicit Value(std::monostate) : storage_(std::monostate{}) {}

    ValueStorage storage_;
};

std::ostream& operator<<(std::ostream& os, const Value& v);

}  // namespace value
