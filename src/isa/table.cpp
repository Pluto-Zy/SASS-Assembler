#include "sassas/isa/table.hpp"

#include <algorithm>
#include <cassert>
#include <iterator>
#include <optional>
#include <ranges>
#include <span>

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
}  // namespace sassas
