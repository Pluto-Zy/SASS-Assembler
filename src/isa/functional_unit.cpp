#include "sassas/isa/functional_unit.hpp"

#include "fmt/base.h"

#include <algorithm>
#include <iterator>
#include <ranges>
#include <string_view>

namespace sassas {
BitMask::BitMask(std::string_view str_description, char zero, char one) {
    for (auto iter = str_description.begin(), end_iter = str_description.end(); iter != end_iter;) {
        iter = std::ranges::find(iter, end_iter, one);
        if (iter != str_description.end()) {
            auto const range_end = static_cast<unsigned>(std::ranges::distance(iter, end_iter));

            iter = std::ranges::find(iter, end_iter, zero);
            auto const range_begin = static_cast<unsigned>(std::ranges::distance(iter, end_iter));

            emplace_back(range_begin, range_end - range_begin);
        }
    }
}

void BitMask::dump() const {
    if (empty()) {
        fmt::print("[Empty]");
    } else {
        auto const print_range = [](BitRange const &range) {
            if (range.size == 1) {
                fmt::print("{}", range.start);
            } else {
                fmt::print("{}-{}", range.start, range.start + range.size - 1);
            }
        };

        fmt::print("[");

        print_range(back());
        for (BitRange const &range : *this | std::views::reverse | std::views::drop(1)) {
            fmt::print(", ");
            print_range(range);
        }

        fmt::print("]");
    }
}

void FunctionalUnit::dump(unsigned indent) const {
    fmt::println("{:>{}}name: {}", "", indent, name_);
    fmt::println("{:>{}}encoding width: {}", "", indent, encoding_width_);

    fmt::println("{:>{}}Bitmasks", "", indent);
    for (auto const &[name, bitmask] : bitmasks_) {
        fmt::print("{:>{}}{}    ", "", indent + 4, name);
        bitmask.dump();
        fmt::print("\n");
    }
}
}  // namespace sassas
