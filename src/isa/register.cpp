#include "sassas/isa/register.hpp"

#include "fmt/base.h"
#include "fmt/format.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <functional>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>

namespace sassas {
auto RegisterGroup::find(std::string_view name) const -> std::optional<unsigned> {
    for (auto const &[reg_name, reg_value] : registers_ | std::views::reverse) {
        // Case-insensitive comparison of the register name with the given name.
        //
        // We found that there are case-insensitive references to register names in TABLES, such
        // as the `DC` category has 2 registers, `nodc` and `DC`, and the reference in TABLES is
        // `DC@noDC`.
        if (std::ranges::equal(
                reg_name,
                name,
                std::ranges::equal_to(),
                static_cast<int (*)(int)>(&std::tolower),
                static_cast<int (*)(int)>(&std::tolower)
            ))
        {
            return reg_value;
        }
    }

    return std::nullopt;
}

auto RegisterGroup::find(unsigned value) const
    -> std::optional<std::reference_wrapper<std::string const>>  //
{
    for (auto const &[reg_name, reg_value] : registers_ | std::views::reverse) {
        if (reg_value == value) {
            return std::cref(reg_name);
        }
    }

    return std::nullopt;
}

void RegisterGroup::dump(unsigned indent) const {
    // Collect the maximum length of the register names.
    std::size_t column_widths[5] {};
    for (std::size_t i = 0; i != registers_.size(); ++i) {
        column_widths[i % 5] = std::ranges::max(column_widths[i % 5], registers_[i].name.size());
    }

    // Dump the register names and values.
    for (std::size_t i = 0; i != registers_.size(); ++i) {
        if (i % 5 == 0) {
            if (i != 0) {
                fmt::println("");
            }

            fmt::print("{:>{}}", "", indent);
        }

        fmt::print(
            "{:<{}} {:<5} ",
            registers_[i].name,
            column_widths[i % 5],
            fmt::format("({})", registers_[i].value)
        );
    }
}
}  // namespace sassas
