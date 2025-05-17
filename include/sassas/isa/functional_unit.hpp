#ifndef SASSAS_ISA_FUNCTIONAL_UNIT_HPP
#define SASSAS_ISA_FUNCTIONAL_UNIT_HPP

#include <cassert>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sassas {
/// Represents a range of bits in a bitmask.
struct BitRange {
    unsigned start;
    unsigned size;

    BitRange() = default;
    BitRange(unsigned start, unsigned size) : start(start), size(size) {
        assert(size != 0 && "BitRange size must be greater than 0");
    }
};

/// Represents a bitmask that can be used to specify which bits in an instruction are relevant for
/// a particular part. The bitmask is defined by a set of `BitRange` objects, each of which
/// specifies a range of bits in the instruction.
class BitMask : public std::vector<BitRange> {
    using Base = std::vector<BitRange>;

public:
    BitMask() = default;
    explicit BitMask(std::vector<BitRange> ranges) : Base(std::move(ranges)) { }
    /// Constructs a BitMask from a string description, where character `zero` represents a bit that
    /// is not set by the mask, and character `one` represents a bit that should be set by the mask.
    ///
    /// This constructor assumes that `str_description` only contains the characters `zero` and
    /// `one`.
    explicit BitMask(std::string_view str_description, char zero = '.', char one = 'X');

    /// Dumps the contents of the bitmask to the standard output. It prints the ranges in reverse
    /// order, so that the least significant bit is printed first. It is used for debugging
    /// purposes.
    void dump() const;
};

class FunctionalUnit {
public:
    FunctionalUnit() = default;

    void set_name(std::string name) {
        name_ = std::move(name);
    }

    auto name() const -> std::string const & {
        return name_;
    }

    void set_encoding_width(unsigned width) {
        encoding_width_ = width;
    }

    auto encoding_width() const -> unsigned {
        return encoding_width_;
    }

    auto add_bitmask(std::string name, BitMask bitmask) -> bool {
        return bitmasks_.try_emplace(std::move(name), std::move(bitmask)).second;
    }

    auto find_bitmask(std::string const &name) const
        -> std::optional<std::reference_wrapper<BitMask const>>  //
    {
        if (auto const iter = bitmasks_.find(name); iter != bitmasks_.end()) {
            return std::cref(iter->second);
        } else {
            return std::nullopt;
        }
    }

    /// Dumps the contents of this object to the standard output. It prints the name and encoding
    /// width of the functional unit, as well as the bitmasks it contains. It is used for debugging
    /// purposes.
    void dump(unsigned indent) const;

private:
    std::string name_;
    unsigned encoding_width_;
    std::unordered_map<std::string, BitMask> bitmasks_;
};
}  // namespace sassas

#endif  // SASSAS_ISA_FUNCTIONAL_UNIT_HPP
