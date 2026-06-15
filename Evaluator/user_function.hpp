#pragma once

#include <cstddef>
#include <map>
#include <memory>
#include <string>

#include "../Parser/ast/expr.hpp"
#include "env_frame.hpp"

namespace eval {

struct UserFunction {
    parser::FunctionDecl* decl = nullptr;
    std::shared_ptr<EnvFrame> closure;
};

using FunctionOverloadMap = std::map<std::size_t, UserFunction>;

}  // namespace eval
