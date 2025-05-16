#include "sassas/isa/table.hpp"

#include "fmt/format.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <iterator>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <vector>

namespace sassas {
auto Table::get_value(std::span<unsigned const> keys) const -> std::optional<unsigned> {
    assert(keys.size() == key_size_ && "Key size mismatch");

    for (auto iter = content_.begin(); iter != content_.end();
         std::ranges::advance(iter, key_size_ + 1))
    {
        bool const match = std::ranges::equal(
            std::views::counted(iter, key_size_),
            keys,
            [](unsigned lhs, unsigned rhs) { return lhs == rhs || lhs == MATCH_ANY; }
        );

        if (match) {
            // The last element in the range is the value.
            return *std::ranges::next(iter, key_size_);
        }
    }

    // No match found.
    return std::nullopt;
}

void Table::dump(unsigned indent) const {
    // The content of the table.
    std::vector<std::string> content_str(content_.size());
    std::ranges::transform(content_, content_str.begin(), [](unsigned value) {
        return value == MATCH_ANY ? "Any" : fmt::format("{}", value);
    });

    // Compute the maximum width of each column.
    std::vector<std::size_t> column_widths(key_size_ + 1);
    for (std::size_t i = 0; i != content_str.size(); i += key_size_ + 1) {
        for (std::size_t j = 0; j != key_size_ + 1; ++j) {
            column_widths[j] = std::ranges::max(column_widths[j], content_str[i + j].size());
        }
    }

    // Print the table.
    for (std::size_t i = 0; i != content_str.size(); i += key_size_ + 1) {
        if (i != 0) {
            fmt::println("");
        }
        fmt::print("{:>{}}", "", indent);

        for (std::size_t j = 0; j != key_size_; ++j) {
            fmt::print("{:>{}}", content_str[i + j], column_widths[j] + 1);
        }

        fmt::print(" -> {:>{}}", content_str[i + key_size_], column_widths[key_size_]);
    }
}
}  // namespace sassas
