#include "sassas/parser/isa_parser.hpp"

#include "sassas/diagnostic/diagnostic.hpp"
#include "sassas/isa/architecture.hpp"
#include "sassas/lexer/token.hpp"

#include "fmt/format.h"

#include "annotate_snippets/annotated_source.hpp"

#include <cassert>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace sassas {
auto ISAParser::create_diag_at_token(
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

auto ISAParser::get_string_literal(Token const &token) -> std::optional<std::string_view> {
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

auto ISAParser::expect_string_literal(Token const &token) -> std::optional<std::string_view> {
    if (token.is(Token::String)) {
        return get_string_literal(token);
    }

    // Invalid token. Generate diagnostic information.
    string_pool_.push_back(
        fmt::format(
            "expected token `{}`, but got `{}`",
            Token::kind_str(Token::String),
            token.kind_str()
        )
    );

    diagnostics_.push_back(create_diag_at_token(
        token,
        DiagLevel::Error,
        "Expected a string literal",
        string_pool_.back()
    ));

    return std::nullopt;
}

auto ISAParser::parse_architecture() -> std::optional<Architecture> {
    assert(
        lexer_.current_token().is(Token::KeywordArchitecture)
        && "Expected ARCHITECTURE keyword at the beginning"
    );

    bool has_errors = false;
    Architecture result;

    // Parse the architecture name.
    if (auto const arch_name = expect_string_literal(lexer_.next_token())) {
        result.name = *arch_name;
    } else {
        has_errors = true;
    }

    // Parse the architecture details.
    while (lexer_.next_token().is(Token::Identifier)) {
        std::string_view const item_name = lexer_.current_token().content();
        Token item_value = lexer_.next_token();

        if (item_value.is(Token::PunctuatorSemi)) {
            // We encountered a semicolon without any content. Generate diagnostic information.
            diagnostics_.push_back(create_diag_at_token(
                item_value,
                DiagLevel::Error,
                "Expected content",
                "missing value for architecture item"
            ));

            has_errors = true;
            continue;
        }

        bool const has_semi = lexer_.lex_until(
            [&](Token const &token) {
                if (token.is(Token::PunctuatorSemi)) {
                    return true;
                } else {
                    item_value = item_value.merge(token, Token::String);
                    return false;
                }
            },
            /*consume=*/false
        );

        if (!has_semi) {
            // We reached the end of the file without encountering a semicolon. Generate diagnostic
            // information.
            diagnostics_.push_back(
                create_diag_at_token(item_value, DiagLevel::Error, "Expected ';'")
            );

            has_errors = true;
            continue;
        }

        result.details.push_back(
            ArchitectureDetail {
                .name = static_cast<std::string>(item_name),
                .value = static_cast<std::string>(item_value.content()),
            }
        );
    }

    if (has_errors) {
        // If there were any errors during parsing, return std::nullopt.
        return std::nullopt;
    } else {
        return result;
    }
}
}  // namespace sassas
