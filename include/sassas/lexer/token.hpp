#ifndef SASSAS_PARSER_TOKEN_HPP
#define SASSAS_PARSER_TOKEN_HPP

#include <cstdint>
#include <string_view>

namespace sassas {
/// This class represents the range of source code occupied by a token.
///
/// This class does not associate with any source code, but only represents a range defined by two
/// positions. In some scenarios, we can store `TokenRange` instead of `token` to avoid the
/// cumbersome construction process.
class TokenRange {
public:
    TokenRange() : location_begin_(0u), location_end_(0u) { }
    TokenRange(unsigned location_begin, unsigned location_end) :
        location_begin_(location_begin), location_end_(location_end) { }

    void set_location_begin(unsigned location) {
        location_begin_ = location;
    }

    auto location_begin() const -> unsigned {
        return location_begin_;
    }

    void set_location_end(unsigned location) {
        location_end_ = location;
    }

    auto location_end() const -> unsigned {
        return location_end_;
    }

    auto size() const -> unsigned {
        return location_end_ - location_begin_;
    }

    /// Returns the content of the token range in the source code.
    auto content(std::string_view source) const -> std::string_view {
        return source.substr(location_begin_, size());
    }

    auto operator==(TokenRange const &other) const -> bool = default;

private:
    unsigned location_begin_;
    unsigned location_end_;
};

class Token {
public:
    enum TokenKind : std::uint8_t {
        /// Unknown token type. For tokens in the source code that are unrecognized or erroneous, we
        /// set them as `Unknown`.
        Unknown,
        /// Marks the end of the file. When the parser reaches the end of the source code, it will
        /// generate an `End` token.
        End,
        /// Represents an identifier. An identifier is a string composed of letters, digits,
        /// underscores and dot, and cannot start with a digit and dot.
        Identifier,
        /// Represents an integer. An integer can be a binary, octal, decimal, or hexadecimal
        /// number.
        Integer,
        /// Represents a string. Strings can be enclosed in single or double quotes. Strings must be
        /// single-line. Currently, we do not support escape characters.
        String,

        // clang-format off
        // Keywords.
        #define SASSAS_KEYWORD(name, spelling) Keyword##name,
        #include "sassas/lexer/keyword.def"

        // Punctuators.
        #define SASSAS_PUNCTUATOR(name, spelling) Punctuator##name,
        #include "sassas/lexer/punctuator.def"
        // clang-format on
    };

    Token() = default;
    Token(TokenKind kind, std::string_view content, unsigned location) :
        kind_(kind), content_(content), location_(location) { }

    void set_kind(TokenKind kind) {
        kind_ = kind;
    }

    auto kind() const -> TokenKind {
        return kind_;
    }

    /// Returns a string description of the token kind. It can be used in the diagnostic message.
    static auto kind_description(TokenKind kind) -> std::string_view;

    /// Returns a string description of the token kind of `*this`.
    auto kind_description() const -> std::string_view {
        return kind_description(kind_);
    }

    void set_content(std::string_view content) {
        content_ = content;
    }

    auto content() const -> std::string_view {
        return content_;
    }

    void set_location_begin(unsigned location) {
        location_ = location;
    }

    auto location_begin() const -> unsigned {
        return location_;
    }

    auto location_end() const -> unsigned {
        return location_ + content_.size();
    }

    auto token_range() const -> TokenRange {
        return TokenRange(location_begin(), location_end());
    }

    auto is(TokenKind kind) const -> bool {
        return kind_ == kind;
    }

    auto is_not(TokenKind kind) const -> bool {
        return kind_ != kind;
    }

    auto is_valid() const -> bool {
        return kind_ != TokenKind::Unknown;
    }

    auto is_keyword() const -> bool;
    auto is_punctuator() const -> bool;

    /// Merges `*this` and `other` into a single token. The merged token has the type of `new_kind`.
    /// Its content is the concatenation of the contents of these two tokens. If there are
    /// whitespace characters between the source code positions of these two tokens, those
    /// whitespace characters will be preserved.
    ///
    /// Note that this method returns the merged token as a new object. It does not modify `*this`
    /// or `other`.
    [[nodiscard]] auto merge(Token const &other, TokenKind new_kind) const -> Token;

    auto operator==(Token const &other) const -> bool = default;

private:
    TokenKind kind_;
    std::string_view content_;
    unsigned location_;
};
}  // namespace sassas

#endif  // SASSAS_PARSER_TOKEN_HPP
