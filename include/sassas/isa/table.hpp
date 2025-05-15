#ifndef SASSAS_ISA_TABLE_HPP
#define SASSAS_ISA_TABLE_HPP

#include <algorithm>
#include <cassert>
#include <iterator>
#include <optional>
#include <ranges>
#include <span>
#include <vector>

namespace sassas {
/// Represents a table in the `TABLES` section of the ISA description file.
///
/// The table is defined as a mapping from an arbitrary number of keys to a single value, where both
/// the keys and the value are unsigned integers. When looking up in the table, we search in order
/// from top to bottom, checking if the key matches the user-provided value. If a key's value is
/// `unsigned(-1)`, it can match any input.
///
/// To store the key and value sequences more efficiently, we store all keys and values in a single
/// `std::vector`. We use `key_size_` to represent the length of the key sequence. For example, for
/// the following table:
///
///     1 0 0 -> 0
///     2 2 0 -> 5
///     2 1 0 -> 5
///
/// we store it as {1, 0, 0, 2, 2, 0, 5, 2, 1, 0, 5}, where `key_size_` is 3.
class Table {
public:
    Table() : key_size_(0) { }
    explicit Table(unsigned key_size) : key_size_(key_size) { }

    void set_key_size(unsigned key_size) {
        key_size_ = key_size;
    }

    auto key_size() const -> unsigned {
        return key_size_;
    }

    void append_item(std::span<unsigned const> keys, unsigned value) {
        assert(keys.size() == key_size_ && "Key size mismatch");

        content_.insert(content_.end(), keys.begin(), keys.end());
        content_.push_back(value);
    }

    auto get_value(std::span<unsigned const> keys) const -> std::optional<unsigned> {
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

private:
    std::vector<unsigned> content_;
    unsigned key_size_;

public:
    static constexpr unsigned MATCH_ANY = static_cast<unsigned>(-1);
};
}  // namespace sassas

#endif  // SASSAS_ISA_TABLE_HPP
