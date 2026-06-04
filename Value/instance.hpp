#pragma once

#include <memory>
#include <string>

namespace parser {
struct ClassDecl;
}

namespace value {

struct EnvFrame;

struct Instance {
    std::shared_ptr<EnvFrame> attrs;
    parser::ClassDecl* type_def = nullptr;
    std::string type_name;
    std::shared_ptr<Instance> self;

    Instance() = default;
    explicit Instance(std::shared_ptr<EnvFrame> env) : attrs(std::move(env)) {}
};

}  // namespace value
