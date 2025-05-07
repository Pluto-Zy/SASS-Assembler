#ifndef SASSAS_ISA_CONDITION_TYPE_HPP
#define SASSAS_ISA_CONDITION_TYPE_HPP

#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_map>

namespace sassas {
/// This class represents the contents of the `CONDITION TYPES` section in the instruction
/// description file, where each item has the following format:
///
///     name : kind
///
/// For example, ILLEGAL_INSTR_ENCODING_ERROR : ERROR.
struct ConditionType {
    enum Kind : unsigned {
        Error,
        Warning,
        Info,
    } kind;
    std::string name;

    /// Creates a `ConditionType` object from the given `kind` and `name`, where the `kind` is a
    /// string representation of the kind of condition type. If the `kind` is not a valid kind, it
    /// returns `std::nullopt`.
    static auto from_string(std::string_view kind, std::string_view name)
        -> std::optional<ConditionType>  //
    {
        if (auto const iter = KIND_STR_MAP.find(kind); iter == KIND_STR_MAP.end()) {
            return std::nullopt;
        } else {
            return ConditionType(iter->second, static_cast<std::string>(name));
        }
    }

    /// Returns a view of all valid kinds of condition types (in string).
    static auto get_kinds() {
        return KIND_STR_MAP | std::views::keys;
    }

private:
    // clang-format off
    static inline std::unordered_map<std::string_view, Kind> const KIND_STR_MAP = {
        { "ERROR", Error },
        { "WARNING", Warning },
        { "INFO", Info },
    };
    // clang-format on

    ConditionType(Kind kind, std::string name) : kind(kind), name(std::move(name)) { }
};
}  // namespace sassas

#endif  // SASSAS_ISA_CONDITION_TYPE_HPP
