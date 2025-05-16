#include "sassas/isa/condition_type.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace sassas {
auto ConditionType::from_string(std::string_view kind, std::string_view name)
    -> std::optional<ConditionType>  //
{
    if (auto const iter = KIND_STR_MAP.find(kind); iter == KIND_STR_MAP.end()) {
        return std::nullopt;
    } else {
        return ConditionType(iter->second, static_cast<std::string>(name));
    }
}
}  // namespace sassas
