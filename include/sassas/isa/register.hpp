#ifndef SASSAS_ISA_REGISTER_HPP
#define SASSAS_ISA_REGISTER_HPP

#include <functional>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sassas {
struct Register {
    std::string name;
    unsigned value;

    Register() = default;
    Register(std::string name, unsigned value) : name(std::move(name)), value(value) { }
};

/// This class represents all registers that belong to the same category, where each register name
/// consists of a "name" and a "value" pair.
///
/// A value may correspond to multiple names, so we must follow a specific search order, which is
/// why we cannot use `map` or `unordered_map`.
class RegisterGroup {
public:
    /// Returns the list of registers in this group. It can be used to dump the contents of this
    /// object.
    auto registers() const -> std::vector<Register> const & {
        return registers_;
    }

    /// Adds a new register to the end of the registers list.
    void append_register(std::string name, unsigned value) {
        registers_.emplace_back(std::move(name), value);
    }

    /// Adds a new register to the end of the registers list. The `value` is optional and defaults
    /// to the last register value + 1. If the list of registers is empty, the default value is 0.
    void append_register(std::string name) {
        append_register(std::move(name), registers_.empty() ? 0 : registers_.back().value + 1);
    }

    /// Concatenates the contents of another `RegisterGroup` object to this one. The `other` object
    /// is moved into this object. Registers in `other` are appended to the end of this object.
    void concat_with(RegisterGroup other) {
        registers_.insert(
            registers_.end(),
            std::make_move_iterator(other.registers_.begin()),
            std::make_move_iterator(other.registers_.end())
        );
    }

    /// Searches for a register by its name. If the register is found, returns its value. Otherwise,
    /// returns `std::nullopt`.
    auto find(std::string_view name) const -> std::optional<unsigned>;

    auto find(std::string const &name) const -> std::optional<unsigned> {
        return find(static_cast<std::string_view>(name));
    }

    /// Searches for a register by its value from the end of the list. Returns the first register
    /// name that matches the value. If no register is found, returns `std::nullopt`.
    auto find(unsigned value) const -> std::optional<std::reference_wrapper<std::string const>>;

    /// Dumps the contents of this object to the standard output. It prints the name and value of
    /// each register in the list. This function prints 5 registers per line and aligns the columns.
    /// It is used for debugging purposes.
    void dump(unsigned indent) const;

private:
    std::vector<Register> registers_;
};
}  // namespace sassas

#endif  // SASSAS_ISA_REGISTER_HPP
