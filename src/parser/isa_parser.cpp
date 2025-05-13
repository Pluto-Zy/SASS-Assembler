#include "sassas/parser/isa_parser.hpp"

#include "sassas/diagnostic/diagnostic.hpp"
#include "sassas/isa/architecture.hpp"
#include "sassas/isa/condition_type.hpp"
#include "sassas/lexer/token.hpp"

#include "fmt/format.h"
#include "fmt/ranges.h"

#include <cassert>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sassas {
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

auto ISAParser::parse_constant_map() -> std::optional<ConstantMap> {
    bool has_errors = false;
    ConstantMap result_map;

    while (lexer_.next_token().is(Token::Identifier)) {
        // The name of the constant.
        Token const name_token = lexer_.current_token();

        // Eat the `=`.
        if (expect_next_token(Token::PunctuatorEqual)) {
            has_errors = true;
            continue;
        }

        // Parse the constant value and insert it into the map.
        if (auto const constant = expect_integer_constant(lexer_.next_token(), 32, true)) {
            std::string name(name_token.content());
            auto const value = static_cast<int>(*constant);

            if (!result_map.try_emplace(std::move(name), value).second) {
                // The constant name already exists in the map. Generate diagnostic information.
                diagnostics_.push_back(
                    create_diag_at_token(name_token, DiagLevel::Error, "Duplicate constant name")
                );

                has_errors = true;
            }
        } else {
            has_errors = true;
        }
    }

    if (has_errors) {
        return std::nullopt;
    } else {
        return result_map;
    }
}

auto ISAParser::parse_parameters() -> std::optional<ConstantMap> {
    assert(
        lexer_.current_token().is(Token::KeywordParameters)
        && "Expected `PARAMETERS` keyword at the beginning"
    );

    return parse_constant_map();
}

auto ISAParser::parse_constants() -> std::optional<ConstantMap> {
    assert(
        lexer_.current_token().is(Token::KeywordConstants)
        && "Expected `CONSTANTS` keyword at the beginning"
    );

    return parse_constant_map();
}

auto ISAParser::parse_string_map() -> std::optional<StringMap> {
    assert(
        lexer_.current_token().is(Token::KeywordStringMap)
        && "Expected `STRING_MAP` keyword at the beginning"
    );

    bool has_errors = false;
    StringMap result_map;

    while (lexer_.next_token().is(Token::Identifier)) {
        // The name of the string.
        Token const name_token = lexer_.current_token();

        // Eat the `->` and checks whether the next token is an identifier.
        if (expect_next_token(Token::PunctuatorArrow) || expect_next_token(Token::Identifier)) {
            has_errors = true;
            continue;
        }

        // Add the item to the map.
        std::string name(name_token.content());
        std::string value(lexer_.current_token().content());

        if (!result_map.try_emplace(std::move(name), std::move(value)).second) {
            // The name already exists in the map. Generate diagnostic information.
            diagnostics_.push_back(
                create_diag_at_token(name_token, DiagLevel::Error, "Duplicate string map item")
            );

            has_errors = true;
        }
    }

    if (has_errors) {
        return std::nullopt;
    } else {
        return result_map;
    }
}
}  // namespace sassas
