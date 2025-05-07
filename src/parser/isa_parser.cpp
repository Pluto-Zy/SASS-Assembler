#include "sassas/parser/isa_parser.hpp"

#include "sassas/diagnostic/diagnostic.hpp"
#include "sassas/isa/architecture.hpp"
#include "sassas/isa/condition_type.hpp"
#include "sassas/lexer/token.hpp"

#include "fmt/format.h"
#include "fmt/ranges.h"

#include "annotate_snippets/annotated_source.hpp"

#include <cassert>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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

auto ISAParser::expect_token(Token const &token, Token::TokenKind expected_kind) -> bool {
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
    if (expect_token(token, Token::String)) {
        return std::nullopt;
    } else {
        return get_string_literal(token);
    }
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

auto ISAParser::parse_condition_types() -> std::optional<std::vector<ConditionType>> {
    assert(
        lexer_.current_token().is(Token::KeywordTypes)
        && "Expected `CONDITION TYPES` keyword at the beginning"
    );

    bool has_errors = false;
    std::vector<ConditionType> results;
    // Parse the list of condition types. Format:
    //
    //     name(identifier) `:` kind(identifier)
    while (lexer_.next_token().is(Token::Identifier)) {
        // The name of the condition type.
        std::string_view const name = lexer_.current_token().content();
        // Eat the `:`, and check if the next token is an identifier.
        if (expect_next_token(Token::PunctuatorColon) || expect_next_token(Token::Identifier)) {
            // TODO: Better error recovery.
            return std::nullopt;
        }
        std::string_view const kind = lexer_.current_token().content();

        if (auto condition_type = ConditionType::from_string(kind, name)) {
            results.push_back(std::move(*condition_type));
        } else {
            // The `kind` string is invaild. Generate diagnostic information.
            string_pool_.push_back(
                fmt::format(
                    "Valid kinds are: {}",
                    fmt::join(
                        ConditionType::get_kinds()
                            | std::views::transform([](std::string_view kind) {
                                  return fmt::format("`{}`", kind);
                              }),
                        ", "
                    )
                )
            );

            diagnostics_.push_back(create_diag_at_token(
                lexer_.current_token(),
                DiagLevel::Error,
                "Invalid kind of condition type",
                {},
                string_pool_.back()
            ));

            has_errors = true;
        }
    }

    if (has_errors) {
        return std::nullopt;
    } else {
        return results;
    }
}
}  // namespace sassas
