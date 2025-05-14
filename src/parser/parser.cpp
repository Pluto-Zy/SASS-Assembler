#include "sassas/parser/parser.hpp"

#include "sassas/diagnostic/diagnostic.hpp"
#include "sassas/lexer/token.hpp"
#include "sassas/utils/unreachable.hpp"

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
#include <string>
#include <string_view>
#include <utility>

namespace sassas {
auto Parser::create_diag_at_token(
    TokenRange target_range,
    DiagLevel level,
    std::string_view message,
    std::string_view label,
    std::string_view note
) const -> Diag {
    auto source =  //
        ants::AnnotatedSource(lexer_.source(), origin_)
            .with_primary_annotation(
                target_range.location_begin(),
                target_range.location_end(),
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

namespace {
/// This is a parser for integer constants. It is used to parse integer constants in the source code
/// and convert them into a `std::uint64_t` value. The parser handles various formats, including
/// decimal, hexadecimal, and binary. It also supports signed and unsigned integers, as well as
/// integer constants with digit separators (e.g., `1_000`).
class IntegerParser {
public:
    struct DiagMessage {
        unsigned annotation_begin;
        unsigned annotation_end;
        std::string message;
    };

    explicit IntegerParser(Token const &token, unsigned bits, bool signedness) :
        content_(token.content()),
        bits_(bits),
        signedness_(signedness),
        cur_pos_(token.location_begin()),
        negative_(false),
        base_(10)  //
    {
        assert(token.is(Token::Integer) && "Expected an integer constant token");
        assert(bits > 0 && bits <= 64 && "Expected a positive integer less than or equal to 64");
        assert(!content_.empty() && "Expected a non-empty integer constant token");
    }

    /// Parses the sign of the integer constant. Stores the sign in the `negative_` member.
    void parse_sign() {
        if (content_.front() == '-') {
            content_.remove_prefix(1);
            ++cur_pos_;

            negative_ = true;
        } else if (content_.front() == '+') {
            content_.remove_prefix(1);
            ++cur_pos_;
        }

        assert((signedness_ || !negative_) && "`negative_` can only be set for signed integers");

        // Remove all whitespace before the sign and the first integer digit, because the sign and
        // integer may be concatenated from two tokens.
        std::size_t const while_space_count = std::ranges::distance(
            content_.begin(),
            std::ranges::find_if_not(content_, static_cast<int (*)(int)>(std::isspace))
        );

        content_.remove_prefix(while_space_count);
        cur_pos_ += while_space_count;
        assert(!content_.empty() && "Expected a non-empty integer constant token");
    }

    /// Parses the base of the integer constant. Stores the base in the `base_` member.
    void parse_base() {
        // The lexer implementation ensures that the first character of this token is an integer.
        if (content_.front() == '0') {
            content_.remove_prefix(1);
            ++cur_pos_;

            if (!content_.empty()) {
                switch (content_.front()) {
                case 'b':
                case 'B':
                    content_.remove_prefix(1);
                    ++cur_pos_;

                    base_ = 2;
                    break;

                case 'x':
                case 'X':
                    content_.remove_prefix(1);
                    ++cur_pos_;

                    base_ = 16;
                    break;

                default:
                    base_ = 8;
                    break;
                }
            }
        }
    }

    /// Checks the validity of digit separators in the integer constant.
    auto check_separator() const -> std::optional<DiagMessage> {
        for (std::size_t i = 0; i != content_.size(); ++i) {
            char const ch = content_[i];

            if (!is_separator(ch)) {
                continue;
            }

            // The digit separator cannot appear at the beginning and end.
            if (i == 0 || i == content_.size() - 1) {
                return DiagMessage {
                    .annotation_begin = static_cast<unsigned>(cur_pos_ + i),
                    .annotation_end = static_cast<unsigned>(cur_pos_ + i + 1),
                    .message = "because the digit separator cannot be here",
                };
            }

            // Cannot use consecutive digit separators.
            if (is_separator(content_[i - 1])) {
                return DiagMessage {
                    .annotation_begin = static_cast<unsigned>(cur_pos_ + i - 1),
                    .annotation_end = static_cast<unsigned>(cur_pos_ + i + 1),
                    .message = "because the digit separator cannot be used consecutively",
                };
            }
        }

        return std::nullopt;
    }

    /// Checks the validity of the digit in the integer constant.
    auto check_digit() const -> std::optional<DiagMessage> {
        for (std::size_t i = 0; i != content_.size(); ++i) {
            char const ch = content_[i];
            if (is_separator(ch)) {
                continue;
            }

            if ((ch < '0' || ch > std::ranges::min('9', static_cast<char>('0' + base_)))
                && (ch < 'a' || ch > std::ranges::min('f', static_cast<char>('a' + base_ - 10)))
                && (ch < 'A' || ch > std::ranges::min('F', static_cast<char>('A' + base_ - 10))))
            {
                return DiagMessage {
                    .annotation_begin = static_cast<unsigned>(cur_pos_ + i),
                    .annotation_end = static_cast<unsigned>(cur_pos_ + i + 1),
                    .message = fmt::format("because this is not a valid digit in base {}", base_),
                };
            }
        }

        return std::nullopt;
    }

    /// Parses the integer constant and returns its value as a `std::uint64_t`. If the integer
    /// constant overflows, returns `std::nullopt`.
    auto parse_integer() const -> std::optional<std::uint64_t> {
        std::uint64_t const max_value = get_max_value();
        std::uint64_t result = 0;

        if (always_fits_into_64bits()) {
            for (char const ch : content_) {
                if (is_separator(ch)) {
                    continue;
                }

                result = result * base_ + digit_value(ch);
            }
        } else {
            for (char const ch : content_) {
                if (is_separator(ch)) {
                    continue;
                }

                bool overflow = false;
                unsigned const digit = digit_value(ch);
                std::uint64_t const old_value = result;

                result *= base_;
                overflow = result / base_ != old_value;

                result += digit;
                overflow |= result < digit;

                if (overflow) {
                    return std::nullopt;
                }
            }
        }

        if (result > max_value) {
            return std::nullopt;
        } else {
            return negative_ ? -result : result;
        }
    }

private:
    /// The content of the integer constant being parsed.
    std::string_view content_;
    /// The maximum number of bits that can be used to represent the integer constant.
    unsigned bits_;
    /// Represents whether the integer constant is signed.
    bool signedness_;
    /// The current position of the character being processed in the source code file. This is used
    /// to report diagnostics.
    unsigned cur_pos_;
    /// The sign of the integer constant. It can be either positive or negative.
    bool negative_;
    /// The base of the integer constant. Available values are 2, 8, 10, and 16.
    unsigned base_;

    static auto is_separator(char ch) -> bool {
        return ch == '_';
    }

    /// Returns the max value of the integer constant based on the number of bits and signedness.
    /// Note that we distinguish the valid range of positive and negative numbers. For negative
    /// numbers, we return the valid range before negation.
    auto get_max_value() const -> std::uint64_t {
        if (negative_) {
            // For negative integers, returns the available range of the value before being negated.
            return static_cast<std::uint64_t>(1) << (bits_ - 1);
        } else if (signedness_) {
            // The range of signed integers is [-2^(bits - 1), 2^(bits - 1) - 1]. Returns the range
            // of non-negative integers.
            return (static_cast<std::uint64_t>(1) << (bits_ - 1)) - 1;
        } else {
            // The range of unsigned integers is [0, 2^bits - 1].
            if (bits_ == 64) {
                return std::numeric_limits<std::uint64_t>::max();
            } else {
                return (static_cast<std::uint64_t>(1) << bits_) - 1;
            }
        }
    }

    /// Returns whether the integer constant can always fit into 64 bits. If so, we can perform the
    /// calculation without worrying about overflow.
    auto always_fits_into_64bits() const -> bool {
        std::size_t const digits = content_.size() - std::ranges::count(content_, '_');
        switch (base_) {
        case 2:
            return digits <= 64;

        case 8:
            return digits <= 64 / 3;

        case 10:
            return digits <= 19;

        case 16:
            return digits <= 64 / 4;

        default:
            unreachable();
        }
    }

    static auto digit_value(char ch) -> unsigned {
        if ('0' <= ch && ch <= '9') {
            return ch - '0';
        } else if ('a' <= ch && ch <= 'f') {
            return ch - 'a' + 10;
        } else if ('A' <= ch && ch <= 'F') {
            return ch - 'A' + 10;
        } else {
            unreachable();
        }
    }
};
}  // namespace

auto Parser::get_integer_constant(Token const &token, unsigned bits, bool signedness)
    -> std::optional<std::uint64_t>  //
{
    IntegerParser parser(token, bits, signedness);
    Diag diag = create_diag_at_token(token, DiagLevel::Error, "Invalid integer constant");

    parser.parse_sign();
    parser.parse_base();

    if (std::optional<IntegerParser::DiagMessage> note;
        // NOLINTNEXTLINE(bugprone-assignment-in-if-condition)
        (note = parser.check_separator()) || (note = parser.check_digit()))
    {
        string_pool_.push_back(std::move(note->message));
        diagnostics_.push_back(
            std::move(diag).with_sub_diag_entry(
                ants::DiagEntry(DiagLevel::Note, string_pool_.back())
                    .with_source(
                        ants::AnnotatedSource(lexer_.source(), origin_)
                            .with_primary_annotation(note->annotation_begin, note->annotation_end)
                    )
            )
        );

        return std::nullopt;
    }

    if (auto value = parser.parse_integer()) {
        return value;
    } else {
        // The integer constant has overflowed. Generate diagnostic information.
        if (signedness) {
            std::int64_t const max = (static_cast<std::int64_t>(1) << (bits - 1)) - 1;
            std::int64_t const min = -(max + 1);

            string_pool_.push_back(fmt::format("the valid range is [{}, {}]", min, max));
        } else {
            string_pool_.push_back(
                fmt::format(
                    "the valid range is [0, {}]",
                    bits == 64 ? std::numeric_limits<std::uint64_t>::max()
                               : (static_cast<std::uint64_t>(1) << bits) - 1
                )
            );
        }

        diagnostics_.push_back(
            std::move(diag)
                .with_sub_diag_entry(DiagLevel::Note, "because the integer constant overflows")
                .with_sub_diag_entry(DiagLevel::Note, string_pool_.back())
        );
        return std::nullopt;
    }
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
