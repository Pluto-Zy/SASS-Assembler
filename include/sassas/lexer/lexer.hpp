#ifndef SASSAS_LEXER_LEXER_HPP
#define SASSAS_LEXER_LEXER_HPP

#include "sassas/lexer/token.hpp"

#include <concepts>
#include <functional>
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

    /// Lexes until the condition `cond` is satisfied. `cond` is a callable object that takes a
    /// `Token` object as an argument and returns a boolean value. The `consume` parameter indicates
    /// whether to consume this token. If a token that satisfies the condition is encountered, it
    /// returns `true`, otherwise it returns `false` (i.e., all tokens are consumed).
    template <class Fn>
        requires std::copy_constructible<Fn> && std::predicate<Fn &, Token const &>
    auto lex_until(Fn cond, bool consume) -> bool {
        while (cur_token_.is_not(Token::End) && !std::invoke(cond, cur_token_)) {
            next_token();
        }

        if (cur_token_.is(Token::End)) {
            return false;
        }

        if (consume) {
            next_token();
        }

        return true;
    }

    /// Lexes until a token of type `kind` is encountered. The `consume` parameter indicates whether
    /// to consume this token. If a token of type `kind` is encountered, it returns `true`,
    /// otherwise it returns `false` (i.e., all tokens are consumed).
    auto lex_until(Token::TokenKind kind, bool consume) -> bool {
        return lex_until([kind](Token const &token) { return token.is(kind); }, consume);
    }

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
