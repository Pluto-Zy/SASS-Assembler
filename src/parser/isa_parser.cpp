#include "sassas/parser/isa_parser.hpp"

#include "sassas/diagnostic/diagnostic.hpp"
#include "sassas/isa/architecture.hpp"
#include "sassas/isa/condition_type.hpp"
#include "sassas/isa/functional_unit.hpp"
#include "sassas/isa/isa.hpp"
#include "sassas/isa/register.hpp"
#include "sassas/isa/table.hpp"
#include "sassas/lexer/lexer.hpp"
#include "sassas/lexer/token.hpp"
#include "sassas/utils/unreachable.hpp"

#include "fmt/format.h"
#include "fmt/ranges.h"

#include "annotate_snippets/annotated_source.hpp"

#include <algorithm>
#include <cassert>
#include <iterator>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sassas {
auto ISAParser::parse() -> std::optional<ISA> {
    // Generate the first token.
    lexer_.next_token();

    bool has_errors = false;
    ISA result;
    while (lexer_.current_token().is_not(Token::End)) {
        // Check that the current token is a keyword.
        if (!lexer_.current_token().is_keyword()) {
            diagnostics_.push_back(create_diag_at_token(
                lexer_.current_token(),
                DiagLevel::Error,
                "Unexpected token",
                add_string(
                    fmt::format(
                        "expected a keyword, but got `{}`",
                        lexer_.current_token().kind_description()
                    )
                )
            ));
            return std::nullopt;
        }

        // NOLINTBEGIN(bugprone-macro-parentheses)
#define PARSE_SECTION(keyword, sec_name)                                                           \
    case Token::Keyword##keyword:                                                                  \
        if (auto sec_name = parse_##sec_name()) {                                                  \
            result.sec_name = std::move(*(sec_name));                                              \
            continue;                                                                              \
        }                                                                                          \
        break;
        // NOLINTEND(bugprone-macro-parentheses)

        switch (lexer_.current_token().kind()) {
            PARSE_SECTION(Architecture, architecture)
            PARSE_SECTION(Parameters, parameters)
            PARSE_SECTION(Constants, constants)
            PARSE_SECTION(StringMap, string_map)
            PARSE_SECTION(Registers, registers)
            PARSE_SECTION(FUnit, functional_unit)

        case Token::KeywordCondition:
            if (!expect_next_token(Token::KeywordTypes)) {
                if (auto condition_types = parse_condition_types()) {
                    result.condition_types = std::move(*condition_types);
                    continue;
                }
            }
            break;

        case Token::KeywordTables:
            if (auto tables = parse_tables(result.registers)) {
                result.tables = std::move(*tables);
                continue;
            }
            break;

        case Token::KeywordOperation:
            if (!expect_next_token(Token::KeywordProperties, Token::KeywordPredicates)) {
                if (lexer_.current_token().is(Token::KeywordProperties)) {
                    if (auto properties = parse_operation_properties()) {
                        result.operation_properties = std::move(*properties);
                        continue;
                    }
                } else {
                    if (auto predicates = parse_operation_predicates()) {
                        result.operation_predicates = std::move(*predicates);
                        continue;
                    }
                }
            }
            break;

        default:
            // We meet a keyword that we cannot parse. Just finish the parsing.
            if (has_errors) {
                // If there were any errors during parsing, return std::nullopt.
                return std::nullopt;
            } else {
                return result;
            }
        }

        // If we reach here, it means that the parsing of the current section failed. We need to
        // recover until the next keyword.
        has_errors = true;
        lexer_.lex_until(&Token::is_keyword, /*consume=*/false);

#undef PARSE_SECTION
    }

    if (has_errors) {
        // If there were any errors during parsing, return std::nullopt.
        return std::nullopt;
    } else {
        return result;
    }
}

auto ISAParser::recover_until(Token::TokenKind expected_kind, bool consume) -> std::nullopt_t {
    bool const match = lexer_.lex_until(expected_kind, consume);
    if (!match) {
        // We reached the end of the file without encountering the expected token. Generate
        // diagnostic information.
        diagnostics_.push_back(create_diag_at_token(
            lexer_.current_token(),
            DiagLevel::Error,
            add_string(fmt::format("Expected `{}`", Token::kind_description(expected_kind)))
        ));
    }

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

auto ISAParser::parse_constant_map() -> std::optional<ISA::ConstantMap> {
    bool has_errors = false;
    ISA::ConstantMap result_map;

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

auto ISAParser::parse_parameters() -> std::optional<ISA::ConstantMap> {
    assert(
        lexer_.current_token().is(Token::KeywordParameters)
        && "Expected `PARAMETERS` keyword at the beginning"
    );

    return parse_constant_map();
}

auto ISAParser::parse_constants() -> std::optional<ISA::ConstantMap> {
    assert(
        lexer_.current_token().is(Token::KeywordConstants)
        && "Expected `CONSTANTS` keyword at the beginning"
    );

    return parse_constant_map();
}

auto ISAParser::parse_string_map() -> std::optional<ISA::StringMap> {
    assert(
        lexer_.current_token().is(Token::KeywordStringMap)
        && "Expected `STRING_MAP` keyword at the beginning"
    );

    bool has_errors = false;
    ISA::StringMap result_map;

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

auto ISAParser::parse_register_category_concatenation(ISA::RegisterTable const &register_table)
    -> std::optional<RegisterGroup>  //
{
    assert(
        lexer_.current_token().is(Token::PunctuatorEqual)
        && "Expected `=` at the beginning of register category concatenation"
    );

    RegisterGroup result;
    // A helper function that appends the list of registers corresponding to the current token
    // (which represents the name of a register category) to the end of `result`. If the category
    // does not exist, it generates diagnostic information and returns `true`.
    auto const concat_category = [&] {
        std::string_view const category_name = lexer_.current_token().content();

        if (auto const iter = register_table.find(static_cast<std::string>(category_name));
            iter != register_table.end())
        {
            // Copy is needed here.
            result.concat_with(iter->second);
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

auto ISAParser::parse_register_list() -> std::optional<RegisterGroup> {
    assert(
        (lexer_.current_token().is(Token::Identifier) || lexer_.current_token().is(Token::String))
        && "Expected register name or string literal at the beginning of register list"
    );

    RegisterGroup result;
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

auto ISAParser::parse_register_category(ISA::RegisterTable const &register_table)
    -> std::optional<RegisterGroup>  //
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

auto ISAParser::parse_registers() -> std::optional<ISA::RegisterTable> {
    assert(
        lexer_.current_token().is(Token::KeywordRegisters)
        && "Expected `REGISTERS` keyword at the beginning"
    );

    ISA::RegisterTable result;
    bool has_errors = false;

    while (lexer_.next_token().is(Token::Identifier)) {
        // The category name.
        Token const category_name_token = lexer_.current_token();
        std::string_view const category_name = category_name_token.content();

        if (std::optional<RegisterGroup> registers = parse_register_category(result)) {
            // The category is valid. Add it to the table.
            // clang-format off
            if (!result.try_emplace(
                    static_cast<std::string>(category_name),
                    std::move(*registers)
                ).second)
            // clang-format on
            {
                // The category name already exists in the table. Generate diagnostic information.
                diagnostics_.push_back(create_diag_at_token(
                    category_name_token,
                    DiagLevel::Error,
                    "Duplicate register category name"
                ));

                has_errors = true;
            }

            if (!expect_current_token(Token::PunctuatorSemi)) {
                // The category is terminated by a semicolon.
                continue;
            }
        }

        // The category is invalid.
        has_errors = true;
        // Lex until the next semicolon.
        recover_until(Token::PunctuatorSemi, /*consume=*/false);
    }

    if (has_errors) {
        return std::nullopt;
    } else {
        return result;
    }
}

auto ISAParser::resolve_table_element(ISA::RegisterTable const &register_table)
    -> std::optional<unsigned>  //
{
    // clang-format off
    if (expect_current_token({
            Token::Integer,
            Token::Identifier,
            Token::String,
            Token::PunctuatorMinus,
        }))
    {
        return std::nullopt;
    }
    // clang-format on

    // A helper class that consumes the current token when we leave the scope.
    struct ConsumeTokenRAII {
        ~ConsumeTokenRAII() {
            lexer.next_token();
        }

        Lexer &lexer;
    } consume_token [[maybe_unused]] { .lexer = lexer_ };

    switch (lexer_.current_token().kind()) {
    case Token::Integer:
        return expect_integer_constant(lexer_.current_token(), 32, false);

    case Token::Identifier:
    case Token::String: {
        // Cases like `Predicate@P0` or `'&'`.

        Token const register_category_token = lexer_.current_token();
        auto const register_category = get_identifier_or_string(register_category_token);
        if (!register_category) {
            return std::nullopt;
        }

        if (lexer_.next_token().is(Token::PunctuatorAt)) {
            auto const register_name = expect_identifier_or_string(lexer_.next_token());
            if (!register_name) {
                return std::nullopt;
            }

            // Get the category of the register.
            if (auto const iter = register_table.find(static_cast<std::string>(*register_category));
                iter != register_table.end())
            {
                // The category exists. Get the value of the register.
                if (auto const value = iter->second.find(*register_name)) {
                    return value;
                } else {
                    // The register name does not exist in the category. Generate diagnostic
                    // information.
                    diagnostics_.push_back(create_diag_at_token(
                        lexer_.current_token(),
                        DiagLevel::Error,
                        "Unknown register name"
                    ));
                }
            } else {
                // The category does not exist. Generate diagnostic information.
                diagnostics_.push_back(create_diag_at_token(
                    register_category_token,
                    DiagLevel::Error,
                    "Unknown register category"
                ));
            }
        } else {
            // Special cases for the `FixLatDestMap` table.
            if (register_category->size() != 1) {
                // The register name is not a single character. Generate diagnostic information.
                diagnostics_.push_back(create_diag_at_token(
                    register_category_token,
                    DiagLevel::Error,
                    "Invalid table element"
                ));
            } else {
                // The register name is a single character. Return its ASCII value.
                return static_cast<unsigned>(register_category->front());
            }
        }

        return std::nullopt;
    }

    case Token::PunctuatorMinus:
        // The `-` token can match any value. We return a special value to indicate this.
        return Table::MATCH_ANY;

    default:
        unreachable();
    }
}

auto ISAParser::parse_single_table(const ISA::RegisterTable &register_table)
    -> std::optional<Table>  //
{
    Table result;
    while (lexer_.current_token().is_not(Token::PunctuatorSemi)) {
        // Parse the key of the table.
        std::vector<unsigned> keys;
        // The token range of each key.
        std::vector<TokenRange> key_ranges;

        while (lexer_.current_token().is_not(Token::PunctuatorArrow)) {
            key_ranges.push_back(lexer_.current_token().token_range());

            if (auto const key = resolve_table_element(register_table)) {
                keys.push_back(*key);
            } else {
                return recover_until(Token::PunctuatorSemi, /*consume=*/false);
            }
        }

        if (result.key_size() == 0) {
            // The table is empty. Set the key size.
            result.set_key_size(keys.size());
        } else if (result.key_size() != keys.size()) {
            // The key size does not match. Generate diagnostic information.
            auto diag = Diag(
                DiagLevel::Error,
                add_string(
                    fmt::format(
                        "The table expects {} key{}, but {} {} provided.",
                        result.key_size(),
                        result.key_size() > 1 ? "s" : "",
                        keys.size(),
                        keys.size() > 1 ? "are" : "is"
                    )
                )
            );

            ants::AnnotatedSource source(lexer_.source(), origin_);
            if (result.key_size() < keys.size()) {
                for (TokenRange const &range : key_ranges | std::views::take(key_ranges.size() - 1)
                         | std::views::drop(result.key_size()))
                {
                    source.add_primary_annotation(range.location_begin(), range.location_end());
                }

                source.add_primary_annotation(
                    key_ranges.back().location_begin(),
                    key_ranges.back().location_end(),
                    "unexpected keys"
                );
            } else {
                source.add_primary_annotation(
                    key_ranges.back().location_end(),
                    key_ranges.back().location_end(),
                    add_string(
                        fmt::format(
                            "missing {} key{}",
                            result.key_size() - keys.size(),
                            result.key_size() - keys.size() > 1 ? "s" : ""
                        )
                    )
                );
            }

            diagnostics_.push_back(std::move(diag).with_source(std::move(source)));
            return recover_until(Token::PunctuatorSemi, /*consume=*/false);
        }

        // Eat the `->`.
        lexer_.next_token();

        // Parse the value.
        if (auto const value = resolve_table_element(register_table)) {
            result.append_item(keys, *value);
        } else {
            return recover_until(Token::PunctuatorSemi, /*consume=*/false);
        }
    }

    return result;
}

auto ISAParser::parse_tables(ISA::RegisterTable const &register_table)
    -> std::optional<ISA::TableMap>  //
{
    assert(
        lexer_.current_token().is(Token::KeywordTables)
        && "Expected `TABLES` keyword at the beginning"
    );

    ISA::TableMap result;
    bool has_errors = false;

    while (lexer_.next_token().is(Token::Identifier)) {
        // The table name.
        Token const table_name_token = lexer_.current_token();
        std::string_view const table_name = table_name_token.content();
        // Consume the token.
        lexer_.next_token();

        // Parse the table.
        if (auto table = parse_single_table(register_table)) {
            if (!result.try_emplace(static_cast<std::string>(table_name), std::move(*table)).second)
            {
                // The table name already exists in the map. Generate diagnostic information.
                diagnostics_.push_back(
                    create_diag_at_token(table_name_token, DiagLevel::Error, "Duplicate table name")
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
        return result;
    }
}

auto ISAParser::parse_identifier_list() -> std::optional<std::vector<std::string>> {
    std::vector<std::string> result;
    do {
        // We are expecting identifier here, but we add the `PunctuatorSemi` token to the list of
        // expected tokens, so that we can generate better diagnostic information.
        if (expect_current_token(Token::Identifier, Token::PunctuatorSemi)) {
            return recover_until(Token::PunctuatorSemi, /*consume=*/true);
        }

        result.emplace_back(lexer_.current_token().content());
    } while (lexer_.next_token().is_not(Token::PunctuatorSemi));

    // Eat the `;`.
    lexer_.next_token();
    return result;
}

auto ISAParser::parse_operation_properties() -> std::optional<std::vector<std::string>> {
    assert(
        lexer_.current_token().is(Token::KeywordProperties)
        && "Expected `OPERATION PROPERTIES` keyword at the beginning"
    );

    // Eat the keyword.
    lexer_.next_token();
    return parse_identifier_list();
}

auto ISAParser::parse_operation_predicates() -> std::optional<std::vector<std::string>> {
    assert(
        lexer_.current_token().is(Token::KeywordPredicates)
        && "Expected `OPERATION PREDICATES` keyword at the beginning"
    );

    // Eat the keyword.
    lexer_.next_token();
    return parse_identifier_list();
}

auto ISAParser::parse_encoding_width() -> std::optional<unsigned> {
    if (auto const val = expect_integer_constant(lexer_.current_token(), 32, false)) {
        auto const encoding_width = static_cast<unsigned>(*val);
        if (encoding_width == 0 || encoding_width > 128) {
            // The encoding width is invalid. Generate diagnostic information.
            diagnostics_.push_back(create_diag_at_token(
                lexer_.current_token(),
                DiagLevel::Error,
                "Invalid encoding width"
            ));
        } else {
            // Eat the `;`.
            if (!expect_next_token(Token::PunctuatorSemi)) {
                return *val;
            }
        }
    }

    // We only return when we succeed in the above code. So if we reach here, it means we failed.
    return recover_until(Token::PunctuatorSemi, /*consume=*/false);
}

auto ISAParser::parse_bitmask(unsigned encoding_width) -> std::optional<BitMask> {
    assert(lexer_.current_token().is(Token::String) && "Expected a string literal token");

    Token const bitmask_token = lexer_.current_token();
    if (auto const bitmask_str = get_string_literal(bitmask_token)) {
        // Check the content of the bitmask.
        if (bitmask_str->size() != encoding_width) {
            diagnostics_.push_back(create_diag_at_token(
                bitmask_token,
                DiagLevel::Error,
                add_string(
                    fmt::format(
                        "The bitmask must be {} bits long, but got {} bits",
                        encoding_width,
                        bitmask_str->size()
                    )
                )
            ));

            return std::nullopt;
        }

        // NOLINTNEXTLINE(readability-qualified-auto)
        auto const invalid_char_iter =
            std::ranges::find_if(*bitmask_str, [](char ch) { return ch != '.' && ch != 'X'; });
        if (invalid_char_iter != bitmask_str->end()) {
            unsigned const char_pos = bitmask_token.location_begin() + 1
                + std::ranges::distance(bitmask_str->begin(), invalid_char_iter);

            diagnostics_.push_back(
                create_diag_at_token(
                    TokenRange(char_pos, char_pos + 1),
                    DiagLevel::Error,
                    add_string(fmt::format("Invalid character `{}` in bitmask", *invalid_char_iter))
                )
                    .with_sub_diag_entry(DiagLevel::Note, "Only `X` and `.` are allowed")
            );
            return std::nullopt;
        }

        return BitMask(*bitmask_str);
    }

    return std::nullopt;
}

auto ISAParser::parse_functional_unit() -> std::optional<FunctionalUnit> {
    assert(
        lexer_.current_token().is(Token::KeywordFUnit)
        && "Expected `FUNIT` keyword at the beginning"
    );

    bool has_errors = false;
    FunctionalUnit result;

    // The name of the functional unit.
    if (expect_next_token(Token::Identifier)) {
        has_errors = true;
    } else {
        result.set_name(static_cast<std::string>(lexer_.current_token().content()));
    }

    // Note that `ENCODING` is a keyword, so we need to specially handle it.
    while (lexer_.next_token().is(Token::Identifier)
           || lexer_.current_token().is(Token::KeywordEncoding))
    {
        // Parse the name of the item, which may consist of multiple identifiers.
        Token item_name = lexer_.current_token();

        // Eat the current token and lex for more identifiers.
        lexer_.next_token();
        lexer_.lex_until(
            [&](Token const &token) {
                if (token.is(Token::Identifier)) {
                    item_name = item_name.merge(token, Token::Identifier);
                    return false;
                } else {
                    return true;
                }
            },
            /*consume=*/false
        );

        if (item_name.content() == "ENCODING WIDTH") {
            // Parse the encoding width.
            if (auto const val = parse_encoding_width()) {
                result.set_encoding_width(*val);
            } else {
                has_errors = true;
            }
        } else {
            // This is not a special item. Check if it is a bitmask.
            if (lexer_.current_token().is(Token::String)) {
                // We recognize string literals as bitmasks.
                if (auto bitmask = parse_bitmask(result.encoding_width())) {
                    // Add the bitmask to the functional unit.
                    if (result.add_bitmask(
                            static_cast<std::string>(item_name.content()),
                            std::move(*bitmask)
                        ))
                    {
                        continue;
                    } else {
                        // Duplicate bitmask name. Generate diagnostic information.
                        diagnostics_.push_back(create_diag_at_token(
                            item_name,
                            DiagLevel::Error,
                            "Duplicate bitmask name"
                        ));
                    }
                }

                has_errors = true;
            } else {
                // For other items, ignore them and lex until the next semicolon.
                recover_until(Token::PunctuatorSemi, /*consume=*/false);
            }
        }
    }

    if (has_errors) {
        return std::nullopt;
    } else {
        return result;
    }
}
}  // namespace sassas
