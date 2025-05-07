#ifndef SASSAS_PARSER_ISA_PARSER_HPP
#define SASSAS_PARSER_ISA_PARSER_HPP

#include "sassas/diagnostic/diagnostic.hpp"
#include "sassas/isa/architecture.hpp"
#include "sassas/isa/condition_type.hpp"
#include "sassas/lexer/lexer.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sassas {
class Token;

class ISAParser {
public:
    explicit ISAParser(std::string_view origin, std::string_view source) :
        origin_(origin), lexer_(source) { }

    auto take_diagnostics() -> std::vector<Diag> {
        return std::move(diagnostics_);
    }

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

private:
    /// The origin of the source code. This is used to generate diagnostic information.
    std::string_view origin_;
    Lexer lexer_;
    /// String pool. Its lifetime is consistent with that of the `ISAParser` object. We need this
    /// string pool because `ants::Diag` itself does not store string objects, only storing their
    /// references. If the strings in the diagnostic information are dynamically generated at
    /// runtime, they need to be placed in the string pool to ensure that they remain valid before
    /// the diagnostic information is issued.
    std::vector<std::string> string_pool_;
    /// Stores all diagnostic information generated during the parsing process.
    std::vector<Diag> diagnostics_;

    /// Creates a `Diag` object and adds a primary annotation at the range of `target`. The
    /// diagnostic has level `level` and carries the message `message`. If `label` is not empty, the
    /// corresponding label is added to the label. If `note` is not empty, an additional diagnostic
    /// item is added to the diagnostic. Note that the created `Diag` object is not added to the
    /// `diagnostics_` vector, so that the user can modify it before adding it to the list.
    auto create_diag_at_token(
        Token const &target,
        DiagLevel level,
        std::string_view message,
        std::string_view label = {},
        std::string_view note = {}
    ) const -> Diag;

    /// Returns whether `token` is **not** of kind `expected_kind`. If the kind does not match, it
    /// generates a diagnostic message and adds it to the `diagnostics_` list.
    auto expect_token(Token const &token, Token::TokenKind expected_kind) -> bool;

    auto expect_current_token(Token::TokenKind expected_kind) -> bool {
        return expect_token(lexer_.current_token(), expected_kind);
    }

    auto expect_next_token(Token::TokenKind expected_kind) -> bool {
        return expect_token(lexer_.next_token(), expected_kind);
    }

    /// Extracts the string content from a `token` that represents a string literal. The returned
    /// value does not include the surrounding quotes. This function checks the validity of the
    /// string represented by `token`. If the string is invalid, it generates diagnostic information
    /// and stores it in `diagnostics_`, returning `std::nullopt`. This function assumes that
    /// `token` is a token representing a string literal; if it is not, the behavior is undefined.
    auto get_string_literal(Token const &token) -> std::optional<std::string_view>;

    /// This function behaves similarly to `get_string_literal(token)`, except that it checks
    /// whether `token` is of type `Token::String`. If not, it generates diagnostic information and
    /// returns `std::nullopt`.
    auto expect_string_literal(Token const &token) -> std::optional<std::string_view>;
};
}  // namespace sassas

#endif  // SASSAS_PARSER_ISA_PARSER_HPP
