#ifndef SASSAS_PARSER_PARSER_HPP
#define SASSAS_PARSER_PARSER_HPP

#include "sassas/diagnostic/diagnostic.hpp"
#include "sassas/lexer/lexer.hpp"
#include "sassas/lexer/token.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sassas {
/// This class provides a generic parser interface for other parser classes to inherit from. It
/// encapsulates a lexer and provides some common parsing functions and interfaces for issuing
/// diagnostic information.
class Parser {
public:
    explicit Parser(std::string_view origin, std::string_view source) :
        origin_(origin), lexer_(source) { }

    /// Takes the diagnostic information generated during the parsing process and returns it as a
    /// vector of `Diag` objects. The returned vector is moved, so the caller should not expect to
    /// retain the original vector after this call.
    auto take_diagnostics() -> std::vector<Diag> {
        return std::move(diagnostics_);
    }

protected:
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

    /// Parses the value of an integer constant from `token`. Currently, it supports binary
    /// (starting with 0b or 0B), octal (starting with 0), decimal, and hexadecimal (starting with
    /// 0x or 0X) integers. It does not support parsing floating-point numbers. The character `_`
    /// can be used to separate digits.
    ///
    /// This function checks whether `token` is a valid integer constant, i.e., it checks whether
    /// its format is correct. In addition, it also verifies whether the integer value can be
    /// represented by `bits` bits of signedness `signedness`. If it is not valid, it generates
    /// diagnostic information and returns `std::nullopt`. `bits` must be a positive integer and
    /// less than or equal to 64. `signedness` is a boolean value indicating whether the integer is
    /// signed.
    ///
    /// We additionally allow the token to represent a value prefixed with a positive or negative
    /// sign, such as +0x1234 or -0x1234. In lexical analysis, we do not consider the sign as part
    /// of the integer literal, because at that time we cannot distinguish between "negative" and
    /// "subtract". However, in some scenarios we need to include the sign, such as LDS R0,
    /// [R1+-0x1]. Therefore, we allow such an extension here. Note that a negative sign can only be
    /// used when `signedness` is `true`.
    ///
    /// This function assumes that `token` is a token representing an integer constant; if it is
    /// not, the behavior is undefined.
    ///
    /// This function returns the integer value in the form of `uint64_t`, i.e., the result after
    /// sign extension to 64 bits.
    auto get_integer_constant(Token const &token, unsigned bits, bool signedness)
        -> std::optional<std::uint64_t>;

    /// This function behaves similarly to `get_integer_constant(token, bits, signedness)`, except
    /// that it checks whether `token` is of type `Token::Integer`. If not, it generates diagnostic
    /// information and returns `std::nullopt`.
    auto expect_integer_constant(Token const &token, unsigned bits, bool signedness)
        -> std::optional<std::uint64_t>;
};
}  // namespace sassas

#endif  // SASSAS_PARSER_PARSER_HPP
