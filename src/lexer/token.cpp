#include "sassas/lexer/token.hpp"

#include <algorithm>
#include <cstddef>
#include <string_view>

namespace sassas {
auto Token::kind_description(TokenKind kind) -> std::string_view {
    switch (kind) {
    case End:
        return "`EOF`";
    case Identifier:
        return "identifier";
    case Integer:
        return "integer";
    case String:
        return "string";

#define SASSAS_KEYWORD(name, spelling)                                                             \
    case TokenKind::Keyword##name:                                                                 \
        return "keyword `" spelling "`";
#include "sassas/lexer/keyword.def"

#define SASSAS_PUNCTUATOR(name, spelling)                                                          \
    case TokenKind::Punctuator##name:                                                              \
        return "`" spelling "`";
#include "sassas/lexer/punctuator.def"

    default:
        return "unknown";
    }
}

auto Token::is_keyword() const -> bool {
    switch (kind_) {
        // clang-format off
    #define SASSAS_KEYWORD(name, spelling) case TokenKind::Keyword##name:
    #include "sassas/lexer/keyword.def"
        // clang-format on
        return true;

    default:
        return false;
    }
}

auto Token::is_punctuator() const -> bool {
    switch (kind_) {
        // clang-format off
    #define SASSAS_PUNCTUATOR(name, spelling) case TokenKind::Punctuator##name:
    #include "sassas/lexer/punctuator.def"
        // clang-format on
        return true;

    default:
        return false;
    }
}

auto Token::merge(Token const &other, TokenKind new_kind) const -> Token {
    unsigned const location = std::ranges::min(location_, other.location());

    char const *const begin =
        location_ < other.location() ? content_.data() : other.content().data();
    std::size_t const size =
        std::ranges::max(location_ + content_.size(), other.location() + other.content().size())
        - location;

    return { new_kind, std::string_view(begin, size), location };
}
}  // namespace sassas
