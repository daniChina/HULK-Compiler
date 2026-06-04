#pragma once

#include <map>
#include <memory>
#include <string>

#include "../Value/value.hpp"

namespace eval {

struct EnvFrame {
    std::map<std::string, value::Value> bindings;
    std::shared_ptr<EnvFrame> parent;

    explicit EnvFrame(std::shared_ptr<EnvFrame> parent_frame = nullptr)
        : parent(std::move(parent_frame)) {}

    void define(const std::string& name, value::Value val) { bindings[name] = std::move(val); }

    value::Value* lookup(const std::string& name) {
        auto it = bindings.find(name);
        if (it != bindings.end()) {
            return &it->second;
        }
        if (parent) {
            return parent->lookup(name);
        }
        return nullptr;
    }
};

}  // namespace eval
