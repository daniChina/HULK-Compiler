#include "value.hpp"

#include "instance.hpp"

namespace value {

std::string Value::toString() const {
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
    if (isInstance()) {
        return "<instance>";
    }
    return "<value>";
}

std::string Value::getTypeName() const {
    if (isString()) {
        return "String";
    }
    if (isNumber()) {
        return "Number";
    }
    if (isBool()) {
        return "Boolean";
    }
    if (isInstance()) {
        return "Instance";
    }
    return "Unknown";
}

std::ostream& operator<<(std::ostream& os, const Value& v) {
    if (v.isNumber()) {
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
