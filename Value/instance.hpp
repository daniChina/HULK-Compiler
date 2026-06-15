#pragma once

#include <memory>
#include <string>

namespace parser {
struct ClassDecl;
}

namespace eval {
struct EnvFrame;
}

namespace value {

struct Instance {
    std::shared_ptr<eval::EnvFrame> attrs;
    parser::ClassDecl* type_def = nullptr;
    std::string type_name;
    std::shared_ptr<Instance> self;

    Instance() = default;
    explicit Instance(std::shared_ptr<eval::EnvFrame> env) : attrs(std::move(env)) {}
};

}  // namespace value
