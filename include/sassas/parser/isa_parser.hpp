#ifndef SASSAS_PARSER_ISA_PARSER_HPP
#define SASSAS_PARSER_ISA_PARSER_HPP

#include "sassas/isa/architecture.hpp"
#include "sassas/isa/condition_type.hpp"
#include "sassas/parser/parser.hpp"

#include <optional>
#include <vector>

namespace sassas {
class ISAParser : public Parser {
public:
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
};
}  // namespace sassas

#endif  // SASSAS_PARSER_ISA_PARSER_HPP
