#include "sassas/lexer/lexer.hpp"

#include "sassas/lexer/token.hpp"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <string_view>
#include <unordered_map>

namespace sassas {
namespace {
auto adjust_identifer_kind(Token &identifier_token) -> Token const & {
    static std::unordered_map<std::string_view, Token::TokenKind> const keywords = {
#define SASSAS_KEYWORD(name, spelling) { spelling, Token::Keyword##name },
#include "sassas/lexer/keyword.def"
    };

    assert(identifier_token.is(Token::Identifier) && "Token is not an identifier");
    if (auto const iter = keywords.find(identifier_token.content()); iter != keywords.end()) {
        identifier_token.set_kind(iter->second);
    }

    return identifier_token;
}

auto lex_string_literal(
    std::string_view::const_iterator current,
    std::string_view::const_iterator end
) -> std::string_view::const_iterator {
    assert(current != end && "Empty string literal token");

    char const quote = *current++;
    // Find the end of the string (the same quote). We do not support multi-line strings, so we also
    // end if we encounter a newline character.
    // clang-format off
    current = std::ranges::find_if(
        current,
        end,
        [&](char ch) { return ch == quote || ch == '\n'; }
    );
    // clang-format on

    if (current != end && *current == quote) {
        // We found the closing quote. Move past it.
        ++current;
    } else {
        // We did not find the closing quote. This is an illegal string token, but we do not throw
        // an error in the lexer. We still treat it as a string token. The parser will check the
        // legality of the string further.
    }

    return current;
}
}  // namespace

auto Lexer::next_token() -> Token const & {
    // Consume whitespace.
    // clang-format off
    current_ = std::ranges::find_if_not(
        current_,
        source_.end(),
        static_cast<int (*)(int)>(std::isspace)
    );
    // clang-format on

    if (current_ == source_.end()) {
        return cur_token_ = form_token(Token::End, current_);
    }

    auto const token_begin = current_;  // NOLINT(readability-qualified-auto)
    // A helper function to create a token. It sets `cur_token_` to the new token and returns it.
    auto const form_token = [&](Token::TokenKind kind) -> auto & {
        return cur_token_ = this->form_token(kind, token_begin);
    };

    switch (*current_++) {
        // clang-format off
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
        // clang-format on
        // Integer literal.
        current_ = std::ranges::find_if_not(current_, source_.end(), [](char c) {
            return std::isalnum(c) || c == '_';
        });
        return form_token(Token::Integer);

        // clang-format off
    case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G': case 'H': case 'I':
    case 'J': case 'K': case 'L': case 'M': case 'N': case 'O': case 'P': case 'Q': case 'R':
    case 'S': case 'T': case 'U': case 'V': case 'W': case 'X': case 'Y': case 'Z': case 'a':
    case 'b': case 'c': case 'd': case 'e': case 'f': case 'g': case 'h': case 'i': case 'j':
    case 'k': case 'l': case 'm': case 'n': case 'o': case 'p': case 'q': case 'r': case 's':
    case 't': case 'u': case 'v': case 'w': case 'x': case 'y': case 'z': case '_':
        // clang-format on
        // Identifier or keyword.
        current_ = std::ranges::find_if_not(current_, source_.end(), [](char c) {
            return std::isalnum(c) || c == '_';
        });
        return adjust_identifer_kind(form_token(Token::Identifier));

    case '"':
    case '\'':
        // String literal.
        current_ = lex_string_literal(--current_, source_.end());
        return form_token(Token::String);

    case '[':
        return form_token(Token::PunctuatorLeftSquare);
    case ']':
        return form_token(Token::PunctuatorRightSquare);
    case '(':
        return form_token(Token::PunctuatorLeftParen);
    case ')':
        return form_token(Token::PunctuatorRightParen);
    case '{':
        return form_token(Token::PunctuatorLeftBrace);
    case '}':
        return form_token(Token::PunctuatorRightBrace);

    case '+':
        return form_token(Token::PunctuatorPlus);
    case '-':
        if (current_ != source_.end() && *current_ == '>') {
            ++current_;
            return form_token(Token::PunctuatorArrow);
        } else {
            return form_token(Token::PunctuatorMinus);
        }
    case '*':
        return form_token(Token::PunctuatorStar);
    case '/':
        return form_token(Token::PunctuatorSlash);
    case '%':
        return form_token(Token::PunctuatorPercent);

    case '~':
        return form_token(Token::PunctuatorTilde);
    case '!':
        if (current_ != source_.end() && *current_ == '=') {
            ++current_;
            return form_token(Token::PunctuatorExclaimEqual);
        } else {
            return form_token(Token::PunctuatorExclaim);
        }
    case '<':
        if (current_ != source_.end()) {
            if (*current_ == '=') {
                ++current_;
                return form_token(Token::PunctuatorLessEqual);
            } else if (*current_ == '<') {
                ++current_;
                return form_token(Token::PunctuatorLessLess);
            }
        }
        return form_token(Token::PunctuatorLess);
    case '>':
        if (current_ != source_.end()) {
            if (*current_ == '=') {
                ++current_;
                return form_token(Token::PunctuatorGreaterEqual);
            } else if (*current_ == '>') {
                ++current_;
                return form_token(Token::PunctuatorGreaterGreater);
            }
        }
        return form_token(Token::PunctuatorGreater);
    case '=':
        if (current_ != source_.end() && *current_ == '=') {
            ++current_;
            return form_token(Token::PunctuatorEqualEqual);
        } else {
            return form_token(Token::PunctuatorEqual);
        }

    case '&':
        if (current_ != source_.end() && *current_ == '&') {
            ++current_;
            return form_token(Token::PunctuatorAmpAmp);
        } else {
            return form_token(Token::PunctuatorAmp);
        }
    case '|':
        if (current_ != source_.end() && *current_ == '|') {
            ++current_;
            return form_token(Token::PunctuatorPipePipe);
        } else {
            return form_token(Token::PunctuatorPipe);
        }

    case '.':
        return form_token(Token::PunctuatorDot);
    case '?':
        return form_token(Token::PunctuatorQuestion);
    case ':':
        return form_token(Token::PunctuatorColon);
    case ';':
        return form_token(Token::PunctuatorSemi);
    case ',':
        return form_token(Token::PunctuatorComma);
    case '@':
        return form_token(Token::PunctuatorAt);
    case '$':
        return form_token(Token::PunctuatorDollar);
    case '`':
        return form_token(Token::PunctuatorBackTick);

    default:
        // Unknown character. We do not throw an error in the lexer. The parser will check the
        // legality of the token further.
        return form_token(Token::Unknown);
    }
}
}  // namespace sassas
