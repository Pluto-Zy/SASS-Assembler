#ifndef SASSAS_PARSER_ISA_PARSER_HPP
#define SASSAS_PARSER_ISA_PARSER_HPP

#include "sassas/isa/architecture.hpp"
#include "sassas/isa/condition_type.hpp"
#include "sassas/parser/parser.hpp"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace sassas {
class ISAParser : public Parser {
public:
    using ConstantMap = std::unordered_map<std::string, int>;
    using StringMap = std::unordered_map<std::string, std::string>;

    using Parser::Parser;

    /// Parses the `ARCHITECTURE` section in the instruction description file. If the parsing is
    /// successful, it returns the parsed `Architecture` object. Otherwise, it returns
    /// `std::nullopt` and the generated diagnostic information can be obtained through the
    /// `take_diagnostics()` method.
    auto parse_architecture() -> std::optional<Architecture>;

    /// Parses the `CONDITION TYPES` section in the instruction description file. If the parsing is
    /// successful, it returns a vector of `ConditionType` objects, which correspond to each item in
    /// this section. Otherwise, it returns `std::nullopt` and the generated diagnostic information
    /// can be obtained through the `take_diagnostics()` method.
    auto parse_condition_types() -> std::optional<std::vector<ConditionType>>;

    /// Parses the `PARAMETERS` section in the instruction description file. If the parsing is
    /// successful, it returns a map of parameter names to their constant values. Otherwise, it
    /// returns `std::nullopt` and the generated diagnostic information can be obtained through the
    /// `take_diagnostics()` method.
    auto parse_parameters() -> std::optional<ConstantMap>;

    /// Parses the `CONSTANTS` section in the instruction description file. It does almost the same
    /// thing as `parse_parameters()`.
    auto parse_constants() -> std::optional<ConstantMap>;

    /// Parses the `STRING_MAP` section in the instruction description file. It contains a mapping
    /// from an identifier to another identifier. If the parsing is successful, it returns a map of
    /// string names to their corresponding string values. Otherwise, it returns `std::nullopt` and
    /// the generated diagnostic information can be obtained through the `take_diagnostics()`
    /// method.
    auto parse_string_map() -> std::optional<StringMap>;

private:
    /// Parses a list of constant mappings, which is a mapping from a string to an integer value.
    /// The `PARAMETERS` and `CONSTANTS` sections have such a list. The list starts with a keyword
    /// token and terminates when the next keyword token is encountered.
    ///
    /// Currently we use `int` as the value type.
    auto parse_constant_map() -> std::optional<ConstantMap>;
};
}  // namespace sassas

#endif  // SASSAS_PARSER_ISA_PARSER_HPP
