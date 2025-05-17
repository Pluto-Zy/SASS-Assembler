#include "sassas/isa/isa.hpp"

#include "sassas/isa/architecture.hpp"
#include "sassas/isa/condition_type.hpp"

#include "fmt/base.h"
#include "fmt/format.h"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <string_view>

namespace sassas {
namespace {
template <class KeyProj, class ValueProj, class Container>
void dump_key_value_pairs(
    std::string_view title,
    Container const &container,
    KeyProj key_proj = {},
    ValueProj value_proj = {},
    std::string_view delimiter = ": "
) {
    fmt::println("{}", title);
    fmt::println("{}", std::string(title.size(), '='));

    for (auto const &elem : container) {
        fmt::println(
            "    {}{}{}",
            std::invoke(key_proj, elem),
            delimiter,
            std::invoke(value_proj, elem)
        );
    }
    fmt::println("");
}

void dump_string_list(std::string_view title, std::vector<std::string> const &list) {
    fmt::print("{}\n{}", title, std::string(title.size(), '='));

    // Print 5 items per line. Compute the maximum width of each column.
    std::size_t column_widths[5] {};
    for (std::size_t i = 0; i != list.size(); ++i) {
        column_widths[i % 5] = std::ranges::max(column_widths[i % 5], list[i].size());
    }

    for (std::size_t i = 0; i != list.size(); ++i) {
        if (i % 5 == 0) {
            fmt::print("\n    ");
        }

        fmt::print("{:<{}} ", list[i], column_widths[i % 5]);
    }

    fmt::print("\n\n");
}
}  // namespace

void ISA::dump() const {
    fmt::println("ISA dump:");

    dump_key_value_pairs(
        fmt::format("Architecture ({})", architecture.name),
        architecture.details,
        &ArchitectureDetail::name,
        [](ArchitectureDetail const &detail) -> decltype(auto) {
            if (detail.value.size() > 65) {
                return fmt::format(
                    "{}... ({} more characters)",
                    static_cast<std::string_view>(detail.value).substr(0, 65),
                    detail.value.size() - 65
                );
            } else {
                return detail.value;
            }
        }
    );

    dump_key_value_pairs(
        "Condition Types",
        condition_types,
        &ConditionType::name,
        [](ConditionType const &type) { return static_cast<unsigned>(type.kind); }
    );

    dump_key_value_pairs(
        "Parameters",
        parameters,
        &ConstantMap::value_type::first,
        &ConstantMap::value_type::second,
        /*delimiter=*/" = "
    );

    dump_key_value_pairs(
        "Constants",
        constants,
        &ConstantMap::value_type::first,
        &ConstantMap::value_type::second,
        /*delimiter=*/" = "
    );

    dump_key_value_pairs(
        "String Map",
        string_map,
        &StringMap::value_type::first,
        &StringMap::value_type::second
    );

    fmt::println("Registers\n=========");
    for (auto const &[category, reg_group] : registers) {
        fmt::println("    {}", category);
        reg_group.dump(8);
        fmt::println("");
    }
    fmt::println("");

    fmt::println("Tables\n=======");
    for (auto const &[name, table] : tables) {
        fmt::println("    {}", name);
        table.dump(8);
        fmt::println("");
    }
    fmt::println("");

    dump_string_list("Operation Properties", operation_properties);
    dump_string_list("Operation Predicates", operation_predicates);

    fmt::println("Functional Unit\n================");
    functional_unit.dump(4);
    fmt::println("");
}
}  // namespace sassas
