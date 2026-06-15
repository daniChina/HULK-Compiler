#include "value.hpp"

#include "instance.hpp"

namespace value {

std::string Value::toString() const {
    if (isNull()) {
        return "Null";
    }
    if (isString()) {
        return asString();
    }
    if (isNumber()) {
        std::ostringstream oss;
        oss << asNumber();
        return oss.str();
    }
    if (isBool()) {
        return asBool() ? "true" : "false";
    }
    if (isRange()) {
        const auto range = asRange();
        return "<range " + std::to_string(static_cast<int>(range->start)) + ".." +
               std::to_string(static_cast<int>(range->end)) + ")>";
    }
    if (isRangeIterator()) {
        return "<iterator>";
    }
    if (isInstance()) {
        return "<instance>";
    }
    return "<value>";
}

std::string Value::getTypeName() const {
    if (isNull()) {
        return "Null";
    }
    if (isString()) {
        return "String";
    }
    if (isNumber()) {
        return "Number";
    }
    if (isBool()) {
        return "Boolean";
    }
    if (isRange() || isRangeIterator()) {
        return "Iterable";
    }
    if (isInstance()) {
        return "Instance";
    }
    return "Unknown";
}

std::ostream& operator<<(std::ostream& os, const Value& v) {
    if (v.isNull()) {
        os << "Null";
    } else if (v.isNumber()) {
        os << v.asNumber();
    } else if (v.isBool()) {
        os << (v.asBool() ? "true" : "false");
    } else if (v.isString()) {
        os << '"' << v.asString() << '"';
    } else {
        os << v.toString();
    }
    return os;
}

}  // namespace value
