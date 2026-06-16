#pragma once

#include <unordered_map>

#include "../Parser/ast/expr.hpp"
#include "../Types/type_info.hpp"

namespace semantic {

// Mapa Expr* → TypeInfo inferido durante Phase2Analyzer (prereq Fase 4 / codegen).
class TypeMap {
public:
    void set(parser::Expr* expr, const TypeInfo& type) {
        if (expr) {
            map_[expr] = type;
        }
    }

    TypeInfo get(parser::Expr* expr) const {
        if (!expr) {
            return TypeInfo(TypeInfo::Kind::Unknown);
        }
        const auto found = map_.find(expr);
        if (found == map_.end()) {
            return TypeInfo(TypeInfo::Kind::Unknown);
        }
        return found->second;
    }

    bool has(parser::Expr* expr) const {
        return expr && map_.find(expr) != map_.end();
    }

    void clear() { map_.clear(); }

    std::size_t size() const { return map_.size(); }

    const std::unordered_map<parser::Expr*, TypeInfo>& entries() const { return map_; }

private:
    std::unordered_map<parser::Expr*, TypeInfo> map_;
};

}  // namespace semantic
