#include "sassas/parser/parser.hpp"

#include "sassas/diagnostic/diagnostic.hpp"
#include "sassas/lexer/token.hpp"

#include "fmt/format.h"

#include "annotate_snippets/annotated_source.hpp"
#include "annotate_snippets/diag.hpp"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <string_view>
#include <utility>

namespace sassas {
auto Parser::create_diag_at_token(
    Token const &target,
    DiagLevel level,
    std::string_view message,
    std::string_view label,
    std::string_view note
) const -> Diag {
    auto source =  //
        ants::AnnotatedSource(lexer_.source(), origin_)
            .with_primary_annotation(
                target.location(),
                target.location() + target.content().size(),
                label
            );

    auto diag = Diag(level, message).with_source(std::move(source));
    if (!note.empty()) {
        diag.add_sub_diag_entry(DiagLevel::Note, note);
    }

    return diag;
}

auto Parser::expect_token(Token const &token, Token::TokenKind expected_kind) -> bool {
    if (token.is_not(expected_kind)) {
        string_pool_.push_back(
            fmt::format(
                "expected {}, but got {}",
                Token::kind_description(expected_kind),
                token.kind_description()
            )
        );

        diagnostics_.push_back(
            create_diag_at_token(token, DiagLevel::Error, "Unexpected token", string_pool_.back())
        );
        return true;
    } else {
        return false;
    }
}

auto Parser::get_string_literal(Token const &token) -> std::optional<std::string_view> {
    assert(token.is(Token::String) && "Expected a string literal token");

    std::string_view content = token.content();
    assert(!content.empty() && "Expected a non-empty string literal token");

    if (content.size() > 1 && content.front() == content.back()) {
        // Remove the surrounding quotes.
        content.remove_prefix(1);
        content.remove_suffix(1);

        return content;
    }

    // Invalid string literal. Generate diagnostic information.
    diagnostics_.push_back(create_diag_at_token(
        token,
        DiagLevel::Error,
        "Invalid string literal",
        "string literal must be enclosed in quotes"
    ));

    return std::nullopt;
}

auto Parser::expect_string_literal(Token const &token) -> std::optional<std::string_view> {
    if (expect_token(token, Token::String)) {
        return std::nullopt;
    } else {
        return get_string_literal(token);
    }
}

auto Parser::get_integer_constant(Token const &token, unsigned bits, bool signedness)
    -> std::optional<std::uint64_t>  //
{
    assert(token.is(Token::Integer) && "Expected an integer constant token");
    assert(bits > 0 && bits <= 64 && "Expected a positive integer less than or equal to 64");

    std::string_view content = token.content();
    assert(!content.empty() && "Expected a non-empty integer constant token");

    // This value records the actual position of the character we are currently processing in the
    // token.
    std::size_t cur_pos = token.location();

    // Determine the sign of the integer constant.
    bool const negative = [&] {
        if (content.front() == '-') {
            content.remove_prefix(1);
            ++cur_pos;
            return true;
        } else if (content.front() == '+') {
            content.remove_prefix(1);
            ++cur_pos;
            return false;
        } else {
            return false;
        }
    }();
    assert((signedness || !negative) && "`negative` can only be set for signed integers");

    // Remove all whitespace before the sign and the first integer digit, because the sign and
    // integer may be concatenated from two tokens.
    std::size_t const while_space_count = std::ranges::distance(
        content.begin(),
        std::ranges::find_if_not(content, static_cast<int (*)(int)>(std::isspace))
    );
    content.remove_prefix(while_space_count);
    cur_pos += while_space_count;
    assert(!content.empty() && "Expected a non-empty integer constant token");

    // Determine the base of the integer constant.
    unsigned const base = [&] {
        // The lexer implementation ensures that the first character of this token is an integer.
        if (content.front() == '0') {
            content.remove_prefix(1);
            ++cur_pos;

            if (!content.empty()) {
                switch (content.front()) {
                case 'b':
                case 'B':
                    content.remove_prefix(1);
                    ++cur_pos;
                    return 2;

                case 'x':
                case 'X':
                    content.remove_prefix(1);
                    ++cur_pos;
                    return 16;

                default:
                    break;
                }
            }

            return 8;
        } else {
            return 10;
        }
    }();

    // Determine the value range of the integer constant.
    auto const [lb, ub] = [&] {
        if (signedness) {
            // The range of signed integers is [-2^(bits - 1), 2^(bits - 1) - 1].
            std::uint64_t const max = (static_cast<std::uint64_t>(1) << (bits - 1)) - 1;
            std::uint64_t const min = -(max + 1);
            return std::pair(min, max);
        } else {
            // The range of unsigned integers is [0, 2^bits - 1].
            std::uint64_t const max = (static_cast<std::uint64_t>(1) << bits) - 1;
            return std::pair(static_cast<std::uint64_t>(0), max);
        }
    }();

    // The result of the integer constant.
    std::uint64_t result = 0;

    // Helper function that checks whether the integer constant is within the valid range.
    auto const check_overflow = [&] {
        if (signedness) {
            auto const target = static_cast<std::int64_t>(negative ? -result : result);
            auto const lower_bound = static_cast<std::int64_t>(lb);
            auto const upper_bound = static_cast<std::int64_t>(ub);

            return lower_bound <= target && target <= upper_bound;
        } else {
            return result <= ub;
        }
    };

    Diag diag = create_diag_at_token(token, DiagLevel::Error, "Invalid integer constant");

    // A RAII class to manage the lifetime of the diagnostic object. It ensures that the diagnostic
    // object is added to the `diagnostics_` vector when it goes out of scope.
    struct AddDiagRAII {
        explicit AddDiagRAII(Diag &diag, Parser &self) : diag_(&diag), self_(self) { }
        ~AddDiagRAII() {
            if (diag_) {
                self_.diagnostics_.push_back(std::move(*diag_));
            }
        }

        void release() {
            diag_ = nullptr;
        }

    private:
        Diag *diag_;
        Parser &self_;
    } add_diag_raii(diag, *this);

    for (std::size_t i = 0; i != content.size(); ++i) {
        char const ch = content[i];

        if (ch == '_') {
            // The digit separator '_' cannot appear at the beginning and end.
            if (i == 0 || i == content.size() - 1) {
                auto note =
                    ants::DiagEntry(DiagLevel::Note, "because the digit separator cannot be here")
                        .with_source(
                            ants::AnnotatedSource(lexer_.source(), origin_)
                                .with_primary_annotation(cur_pos + i, cur_pos + i + 1)
                        );

                diag.add_sub_diag_entry(std::move(note));
                return std::nullopt;
            }

            // Cannot use consecutive digit separators.
            if (content[i - 1] == '_') {
                auto note =  //
                    ants::DiagEntry(
                        DiagLevel::Note,
                        "because the digit separator cannot be used consecutively"
                    )
                        .with_source(
                            ants::AnnotatedSource(lexer_.source(), origin_)
                                .with_primary_annotation(cur_pos + i - 1, cur_pos + i + 1)
                        );

                diag.add_sub_diag_entry(std::move(note));
                return std::nullopt;
            }

            // In other cases, we can ignore the digit separator.
            continue;
        }

        // Check the validity of the digit.
        unsigned digit;
        if ('0' <= ch && ch <= std::ranges::min('9', static_cast<char>('0' + base))) {
            digit = ch - '0';
        } else if ('a' <= ch && ch <= std::ranges::min('f', static_cast<char>('a' + base - 10))) {
            digit = ch - 'a' + 10;
        } else if ('A' <= ch && ch <= std::ranges::min('F', static_cast<char>('A' + base - 10))) {
            digit = ch - 'A' + 10;
        } else {
            string_pool_.push_back(
                fmt::format("because this is not a valid digit in base {}", base)
            );

            auto note =  //
                ants::DiagEntry(DiagLevel::Note, string_pool_.back())
                    .with_source(
                        ants::AnnotatedSource(lexer_.source(), origin_)
                            .with_primary_annotation(cur_pos + i, cur_pos + i + 1)
                    );

            diag.add_sub_diag_entry(std::move(note));
            return std::nullopt;
        }

        std::uint64_t const value = result * base + digit;
        if (value >= result) {
            result = value;

            if (check_overflow()) {
                continue;
            }
        }

        // If we reach here, it means the integer constant has overflowed.
        diag.add_sub_diag_entry(DiagLevel::Note, "because the integer constant overflows");
        if (signedness) {
            string_pool_.push_back(
                fmt::format(
                    "the valid range is [{}, {}]",
                    static_cast<std::int64_t>(lb),
                    static_cast<std::int64_t>(ub)
                )
            );
        } else {
            string_pool_.push_back(fmt::format("the valid range is [{}, {}]", lb, ub));
        }

        diag.add_sub_diag_entry(DiagLevel::Note, string_pool_.back());
        return std::nullopt;
    }

    add_diag_raii.release();
    return negative ? -result : result;
}

auto Parser::expect_integer_constant(Token const &token, unsigned bits, bool signedness)
    -> std::optional<std::uint64_t>  //
{
    if (expect_token(token, Token::Integer)) {
        return std::nullopt;
    } else {
        return get_integer_constant(token, bits, signedness);
    }
}
}  // namespace sassas
