#ifndef SASSAS_PARSER_ISA_PARSER_HPP
#define SASSAS_PARSER_ISA_PARSER_HPP

#include "sassas/isa/architecture.hpp"
#include "sassas/isa/condition_type.hpp"
#include "sassas/isa/register.hpp"
#include "sassas/isa/table.hpp"
#include "sassas/lexer/token.hpp"
#include "sassas/parser/parser.hpp"

#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace sassas {
class ISAParser : public Parser {
public:
    using ConstantMap = std::unordered_map<std::string, int>;
    using StringMap = std::unordered_map<std::string, std::string>;
    using TableMap = std::unordered_map<std::string, Table>;

    using Parser::Parser;

private:
    /// This function is used for error recovery. It will lex until it encounters a token of the
    /// specified type. If it does not encounter the token, it generates diagnostic information. The
    /// function always returns `std::nullopt`.
    auto recover_until(Token::TokenKind expected_kind, bool consume) -> std::nullopt_t;

public:
    /// Parses the `ARCHITECTURE` section in the instruction description file. If the parsing is
    /// successful, it returns the parsed `Architecture` object. Otherwise, it returns
    /// `std::nullopt` and the generated diagnostic information can be obtained through the
    /// `take_diagnostics()` method.
    auto parse_architecture() -> std::optional<Architecture>;

    /// Parses the `CONDITION TYPES` section in the instruction description file. If the parsing is
    /// successful, it returns a vector of `ConditionType` objects, which correspond to each item in
    /// this section. Otherwise, it returns `std::nullopt` and the generated diagnostic information
    /// can be obtained through the `take_diagnostics()` method.
    auto parse_condition_types() -> std::optional<std::vector<ConditionType>>;

private:
    /// Parses a list of constant mappings, which is a mapping from a string to an integer value.
    /// The `PARAMETERS` and `CONSTANTS` sections have such a list. The list starts with a keyword
    /// token and terminates when the next keyword token is encountered.
    ///
    /// Currently we use `int` as the value type.
    auto parse_constant_map() -> std::optional<ConstantMap>;

public:
    /// Parses the `PARAMETERS` section in the instruction description file. If the parsing is
    /// successful, it returns a map of parameter names to their constant values. Otherwise, it
    /// returns `std::nullopt` and the generated diagnostic information can be obtained through the
    /// `take_diagnostics()` method.
    auto parse_parameters() -> std::optional<ConstantMap>;

    /// Parses the `CONSTANTS` section in the instruction description file. It does almost the same
    /// thing as `parse_parameters()`.
    auto parse_constants() -> std::optional<ConstantMap>;

    /// Parses the `STRING_MAP` section in the instruction description file. It contains a mapping
    /// from an identifier to another identifier. If the parsing is successful, it returns a map of
    /// string names to their corresponding string values. Otherwise, it returns `std::nullopt` and
    /// the generated diagnostic information can be obtained through the `take_diagnostics()`
    /// method.
    auto parse_string_map() -> std::optional<StringMap>;

private:
    /// Parses a register category concatenation, which is a list of register categories
    /// concatenated by `+` signs. For example:
    ///
    ///     Integer = Integer8 + Integer16 + Integer32 + Integer64;
    ///             ^ -------------------------------------------- parse this sequence
    ///             |
    ///             current token
    ///
    /// This function also checks whether the register category exists in `register_table`. If it
    /// does not exist, it returns `std::nullopt`. Otherwise, it returns a `Registers` object that
    /// represents the concatenated register list.
    auto parse_register_category_concatenation(RegisterTable const &register_table)
        -> std::optional<Registers>;

    /// Represents a range expression of the form `(begin..end)`, where `begin` and `end` are both
    /// 32-bit unsigned integers.
    class RangeExpr : public TokenRange, public std::ranges::iota_view<unsigned, unsigned> {
        using IotaBase = std::ranges::iota_view<unsigned, unsigned>;

    public:
        RangeExpr() = default;
        RangeExpr(unsigned begin, unsigned end) : IotaBase(std::views::iota(begin, end + 1)) { }
        RangeExpr(TokenRange token_range, unsigned begin, unsigned end) :
            TokenRange(token_range), IotaBase(std::views::iota(begin, end + 1)) { }

        // There is also a `size()` method `TokenRange` class, so we introduce the needed `size()`
        // here.
        using IotaBase::size;

        /// Creates a range expression that represents an empty range.
        static auto empty_range() -> RangeExpr {
            return { /*begin=*/1, /*end=*/0 };
        }
    };

    /// Parses a range expression of the form `(begin..end)`, where `begin` and `end` are both
    /// 32-bit unsigned integers. Returns a `std::pair` object containing the start and end values.
    /// It also checks whether `begin <= end` holds.
    ///
    /// The function assumes that the current token is `(`. And the current token is the token after
    /// the `)` when the function returns.
    auto parse_range_expr() -> std::optional<RangeExpr>;

    /// Represents a register name in a register list. This object is the return value of
    /// `parse_register_name()`.
    struct RegisterName : public TokenRange {
        /// The prefix of the register name. If the name has a range expression, such as
        /// `SR(0..255)`, then the prefix is `SR`. Otherwise, it is the name of the register.
        std::string_view prefix;
        /// The range expression associated with this name. If the name is not associated with a
        /// range expression, then `range.empty()` returns `true`.
        RangeExpr range;

        RegisterName() = default;
        RegisterName(TokenRange token_range, std::string_view prefix) :
            TokenRange(token_range), prefix(prefix), range(RangeExpr::empty_range()) { }
        RegisterName(TokenRange token_range, std::string_view prefix, RangeExpr range) :
            TokenRange(token_range), prefix(prefix), range(range) { }

        auto has_associated_range() const -> bool {
            return !range.empty();
        }

        /// Returns the number of names represented by this object. If the name has an associated
        /// range expression, the size of the range is returned. Otherwise, the number of names
        /// is 1.
        auto name_count() const -> unsigned {
            return has_associated_range() ? range.size() : 1;
        }
    };

    /// Parses the register name part of a register initialization list, such as:
    ///
    ///     Integer8 U8 = 0, S8 = 1;
    ///              ^^      ^^ parse these two parts
    ///              |
    ///              current token
    ///
    /// If the register name has an associated range expression, it will be included as well.
    ///
    /// This method assumes that the current token is the name itself.
    auto parse_register_name() -> std::optional<RegisterName>;

    /// Parses the initial value of a register, which is the part after the `=` sign in a register
    /// initialization list. It can be an integer or a range expression. If the returned `RangeExpr`
    /// has a size of 1, it indicates a single integer value.
    ///
    /// This method assumes that the current token is the token after the `=` sign.
    auto parse_register_value() -> std::optional<RangeExpr>;

    /// Parses a register initialization list, which is a list of register names and their initial
    /// values, such as
    ///
    ///     Integer8 U8 = 0, S8 = 1;
    ///              ^^^^^^^^^^^^^^ parse this part
    auto parse_register_list() -> std::optional<Registers>;

    /// Parses a register category, which is the name of the category followed by a register list.
    /// For example:
    ///
    ///     Integer8 U8 = 0, S8 = 1;
    ///     ^^^^^^^^^^^^^^^^^^^^^^^ parse this part
    ///
    /// or
    ///
    ///     Integer = Integer8 + Integer16 + Integer32 + Integer64;
    ///     ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ parse this part
    ///
    /// The semicolon at the end of the line is not included in the parsing.
    ///
    /// If an error occurs during the parsing of each part, this function will also perform error
    /// recovery.
    auto parse_register_category(RegisterTable const &register_table) -> std::optional<Registers>;

public:
    /// Parses the `REGISTERS` section in the instruction description file. If the parsing is
    /// successful, it returns a `RegisterTable` object that contains the parsed register
    /// categories. Otherwise, it returns `std::nullopt` and the generated diagnostic information
    /// can be obtained through the `take_diagnostics()` method.
    auto parse_registers() -> std::optional<RegisterTable>;

private:
    /// Parses an element in a table. The element can be either a key or a value in the table. The
    /// function converts the parsed element to an integer and returns it.
    ///
    /// The element in the table can be one of the following types:
    ///
    /// - Integer literal.
    /// - Access to a specific register, such as `AVRG@noAVRG`. The parts on both sides of the `@`
    ///   can be identifiers or strings.
    /// - Token `-`, it seems that the character can match any value.
    /// - A single string literal, which only appears in the `FixLatDestMap` table. The meaning of
    ///   this content is currently unclear, so we only return the ASCII value of the character as
    ///   an integer.
    ///
    /// This function will generate diagnostic information if the resolving fails.
    auto resolve_table_element(RegisterTable const &register_table) -> std::optional<unsigned>;

    /// Parses a single table in the `TABLES` section. It only parses the content of the table, not
    /// the name of the table. The function assumes that the current token is the token after the
    /// table name.
    auto parse_single_table(RegisterTable const &register_table) -> std::optional<Table>;

public:
    /// Parses the `TABLES` section in the instruction description file. All registers in the table
    /// will be resolved to their corresponding values in the `register_table` object. If the
    /// parsing is successful, it returns a map of table names to their corresponding `Table`
    /// objects. Otherwise, it returns `std::nullopt` and the generated diagnostic information can
    /// be obtained through the `take_diagnostics()` method.
    auto parse_tables(RegisterTable const &register_table) -> std::optional<TableMap>;

private:
    /// Parses an identifier list that ends with a semicolon. The identifiers are separated by
    /// spaces. This function is used to implement the parsing of the `OPERATION PROPERTIES` and
    /// `OPERATION PREDICATES` sections.
    ///
    /// This function assumes that the current token is the first token of the identifier list.
    auto parse_identifier_list() -> std::optional<std::vector<std::string>>;

public:
    /// Parses the `OPERATION PROPERTIES` section, which is a list of identifiers separated by
    /// spaces. The list ends with a semicolon. If the parsing is successful, it returns a vector of
    /// identifiers.
    auto parse_operation_properties() -> std::optional<std::vector<std::string>>;

    /// Parses the `OPERATION PREDICATES` section, which is a list of identifiers separated by
    /// spaces. The list ends with a semicolon. If the parsing is successful, it returns a vector of
    /// identifiers.
    auto parse_operation_predicates() -> std::optional<std::vector<std::string>>;
};
}  // namespace sassas

#endif  // SASSAS_PARSER_ISA_PARSER_HPP
