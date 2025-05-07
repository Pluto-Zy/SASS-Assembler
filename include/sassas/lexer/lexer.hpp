#ifndef SASSAS_LEXER_LEXER_HPP
#define SASSAS_LEXER_LEXER_HPP

#include "sassas/lexer/token.hpp"

#include <iterator>
#include <string_view>

namespace sassas {
class Lexer {
public:
    explicit Lexer(std::string_view source) : source_(source), current_(source.begin()) { }

    auto source() const -> std::string_view {
        return source_;
    }

    auto current_token() const -> Token const & {
        return cur_token_;
    }

    /// Produces a token starting from the current position `current_` is pointing to. Moves
    /// `current_` to the end of the token. The generated token is stored in `cur_token_` and
    /// returned by this function.
    auto next_token() -> Token const &;

private:
    /// The content of the source code.
    std::string_view source_;
    /// An iterator to the next character to be processed.
    std::string_view::const_iterator current_;
    /// Caches the last parsed token.
    Token cur_token_;

    /// Creates a `Token` object of type `kind`. The range of the token is [begin, current_).
    auto form_token(Token::TokenKind kind, std::string_view::const_iterator begin) const -> Token {
        return {
            kind,
            std::string_view(begin, current_),
            static_cast<unsigned>(std::ranges::distance(source_.begin(), begin)),
        };
    }
};
}  // namespace sassas

#endif  // SASSAS_LEXER_LEXER_HPP
