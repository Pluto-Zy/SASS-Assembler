#include "sassas/parser/isa_parser.hpp"

#include "sassas/diagnostic/diagnostic.hpp"
#include "sassas/isa/architecture.hpp"
#include "sassas/isa/condition_type.hpp"
#include "sassas/isa/register.hpp"
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
            diagnostics_.push_back(create_diag_at_token(
                lexer_.current_token(),
                DiagLevel::Error,
                "Invalid kind of condition type",
                {},
                add_string(
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
                )
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

auto ISAParser::parse_register_category_concatenation(RegisterTable const &register_table)
    -> std::optional<Registers>  //
{
    assert(
        lexer_.current_token().is(Token::PunctuatorEqual)
        && "Expected `=` at the beginning of register category concatenation"
    );

    Registers result;
    // A helper function that appends the list of registers corresponding to the current token
    // (which represents the name of a register category) to the end of `result`. If the category
    // does not exist, it generates diagnostic information and returns `true`.
    auto const concat_category = [&] {
        std::string_view const category_name = lexer_.current_token().content();

        if (auto registers = register_table.find(static_cast<std::string>(category_name))) {
            // Copy is needed here.
            result.concat_with(*registers);
            return false;
        } else {
            // The category name is not found in the register table. Generate diagnostic
            // information.
            diagnostics_.push_back(create_diag_at_token(
                lexer_.current_token(),
                DiagLevel::Error,
                "Unknown register category"
            ));
            return true;
        }
    };

    do {
        // If the next token is not an identifier, or the category name is not found in the register
        // table, we need to generate diagnostic information.
        if (expect_next_token(Token::Identifier) || concat_category()) {
            return std::nullopt;
        }
    } while (lexer_.next_token().is(Token::PunctuatorPlus));

    return result;
}

auto ISAParser::parse_range_expr() -> std::optional<RangeExpr> {
    assert(
        lexer_.current_token().is(Token::PunctuatorLeftParen)
        && "Expected `(` at the beginning of range expression"
    );

    TokenRange expr_range;
    // The starting point of the range expression.
    expr_range.set_location_begin(lexer_.current_token().location_begin());

    auto const begin = expect_integer_constant(lexer_.next_token(), 32, false);
    if (!begin || expect_next_token(Token::PunctuatorDotDot)) {
        return std::nullopt;
    }

    auto const end = expect_integer_constant(lexer_.next_token(), 32, false);
    if (!end || expect_next_token(Token::PunctuatorRightParen)) {
        return std::nullopt;
    }

    // The ending point of the range expression.
    expr_range.set_location_end(lexer_.current_token().location_end());
    // Eat the `)`.
    lexer_.next_token();

    if (*begin > *end) {
        // The range is invalid. Generate diagnostic information.
        diagnostics_.push_back(create_diag_at_token(
            expr_range,
            DiagLevel::Error,
            "The start of the range is greater than the end"
        ));
        return std::nullopt;
    }

    return RangeExpr(expr_range, *begin, *end);
}

auto ISAParser::parse_register_name() -> std::optional<RegisterName> {
    // The starting point of the register name.
    unsigned const name_begin = lexer_.current_token().location_begin();

    std::optional<std::string_view> const name = get_identifier_or_string(lexer_.current_token());
    if (!name) {
        return std::nullopt;
    }

    switch (lexer_.next_token().kind()) {
    case Token::PunctuatorLeftParen: {
        // Cases like `SR(0..255)`.
        std::optional<RangeExpr> const range = parse_range_expr();
        if (!range) {
            return std::nullopt;
        }

        return RegisterName(TokenRange(name_begin, range->location_end()), *name, *range);
    }

    case Token::PunctuatorStar:
        // Consume the `*` without doing anything with it. Currently we don't know the meaning of
        // `*` in the context of register names.
        lexer_.next_token();
        [[fallthrough]];

    default:
        return RegisterName(TokenRange(name_begin, lexer_.current_token().location_end()), *name);
    }
}

auto ISAParser::parse_register_value() -> std::optional<RangeExpr> {
    if (expect_current_token(Token::PunctuatorLeftParen, Token::Integer)) {
        return std::nullopt;
    }

    if (lexer_.current_token().is(Token::PunctuatorLeftParen)) {
        // Cases like `(0..255)`.
        return parse_range_expr();
    } else {
        Token const integer_token = lexer_.current_token();

        if (auto const value = expect_integer_constant(integer_token, 32, false)) {
            // Eat the integer token.
            lexer_.next_token();
            return RangeExpr(integer_token.token_range(), *value, *value);
        } else {
            return std::nullopt;
        }
    }
}

auto ISAParser::parse_register_list() -> std::optional<Registers> {
    assert(
        (lexer_.current_token().is(Token::Identifier) || lexer_.current_token().is(Token::String))
        && "Expected register name or string literal at the beginning of register list"
    );

    Registers result;
    while (true) {
        std::optional<RegisterName> names = parse_register_name();
        if (!names) {
            return std::nullopt;
        }

        if (lexer_.current_token().is(Token::PunctuatorEqual)) {
            // Eat the `=`.
            lexer_.next_token();

            // Parse the initial value of the register.
            std::optional<RangeExpr> const values = parse_register_value();
            if (!values) {
                return std::nullopt;
            }

            // The size of the register names that this token represents.
            unsigned const name_count = names->name_count();
            // The size of the register values that this token represents.
            unsigned const value_count = values->size();

            if (name_count != value_count) {
                // The number of register names and values do not match. Generate diagnostic
                // information.
                auto source =
                    ants::AnnotatedSource(lexer_.source(), origin_)
                        .with_primary_annotation(
                            names->location_begin(),
                            names->location_end(),
                            add_string(
                                fmt::format("{} name{}", name_count, name_count > 1 ? "s" : "")
                            )
                        )
                        .with_primary_annotation(
                            values->location_begin(),
                            values->location_end(),
                            add_string(
                                fmt::format("{} value{}", value_count, value_count > 1 ? "s" : "")
                            )
                        );

                diagnostics_.push_back(
                    Diag(
                        DiagLevel::Error,
                        "The number of register names and initial values do not match"
                    )
                        .with_source(std::move(source))
                );

                return std::nullopt;
            }

            if (names->has_associated_range()) {
                // TODO: Use `std::views::zip` if the compiler supports.
                for (unsigned i = 0; i != name_count; ++i) {
                    unsigned const name_idx = names->range[i];
                    unsigned const value = (*values)[i];

                    result.append_register(fmt::format("{}{}", names->prefix, name_idx), value);
                }
            } else {
                result.append_register(static_cast<std::string>(names->prefix), values->front());
            }
        } else {
            if (names->has_associated_range()) {
                for (unsigned const name_idx : names->range) {
                    result.append_register(fmt::format("{}{}", names->prefix, name_idx));
                }
            } else {
                result.append_register(static_cast<std::string>(names->prefix));
            }
        }

        if (lexer_.current_token().is(Token::PunctuatorComma)) {
            // Eat the `,`.
            lexer_.next_token();
        } else {
            break;
        }
    }

    return result;
}

auto ISAParser::parse_register_category(RegisterTable const &register_table)
    -> std::optional<Registers>  //
{
    assert(lexer_.current_token().is(Token::Identifier) && "Expected register category name");

    if (expect_next_token(Token::Identifier, Token::PunctuatorEqual, Token::String)) {
        return std::nullopt;
    }

    if (lexer_.current_token().is(Token::PunctuatorEqual)) {
        // The next token is `=`, which means that the category is defined by combining other
        // categories, e.g.
        //     Integer = Integer8 + Integer16 + Integer32 + Integer64;
        return parse_register_category_concatenation(register_table);
    } else {
        // Parse the list of registers.
        return parse_register_list();
    }
}

auto ISAParser::parse_registers() -> std::optional<RegisterTable> {
    assert(
        lexer_.current_token().is(Token::KeywordRegisters)
        && "Expected `REGISTERS` keyword at the beginning"
    );

    RegisterTable result;
    bool has_errors = false;

    while (lexer_.next_token().is(Token::Identifier)) {
        // The category name.
        std::string_view const category_name = lexer_.current_token().content();

        if (std::optional<Registers> registers = parse_register_category(result)) {
            // The category is valid. Add it to the table.
            result.add_register_category(
                static_cast<std::string>(category_name),
                std::move(*registers)
            );

            if (!expect_current_token(Token::PunctuatorSemi)) {
                // The category is terminated by a semicolon.
                continue;
            }
        }

        // The category is invalid.
        has_errors = true;
        // Lex until the next semicolon.
        bool const has_semi = lexer_.lex_until(Token::PunctuatorSemi, /*consume=*/false);
        if (!has_semi) {
            // We reached the end of the file without encountering a semicolon. Generate
            // diagnostic information.
            diagnostics_.push_back(
                create_diag_at_token(lexer_.current_token(), DiagLevel::Error, "Expected ';'")
            );
        }
    }

    if (has_errors) {
        return std::nullopt;
    } else {
        return result;
    }
}
}  // namespace sassas
