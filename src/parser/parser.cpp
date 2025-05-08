#include "sassas/parser/parser.hpp"

#include "sassas/diagnostic/diagnostic.hpp"
#include "sassas/lexer/token.hpp"

#include "fmt/format.h"

#include "annotate_snippets/annotated_source.hpp"

#include <cassert>
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
}  // namespace sassas
