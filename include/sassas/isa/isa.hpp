#ifndef SASSAS_ISA_ISA_HPP
#define SASSAS_ISA_ISA_HPP

#include "sassas/isa/architecture.hpp"
#include "sassas/isa/condition_type.hpp"
#include "sassas/isa/register.hpp"
#include "sassas/isa/table.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace sassas {
/// This class summarizes all the information parsed from the ISA description file. It is produced
/// by `ISAParser`.
struct ISA {
    using ConstantMap = std::unordered_map<std::string, int>;
    using StringMap = std::unordered_map<std::string, std::string>;
    using TableMap = std::unordered_map<std::string, Table>;

    /// The architecture information related to the ISA, represented as a series of key-value pairs
    /// with `name` and `value`. It is parsed from the `ARCHITECTURE` section.
    Architecture architecture;
    /// The condition types defined in the ISA, represented as a vector of `ConditionType` objects,
    /// which is a condition name and its corresponding kind. It is parsed from the `CONDITION
    /// TYPES` section.
    std::vector<ConditionType> condition_types;
    /// Represents a mapping from a string to an integer value. It is used to represent the
    /// `PARAMETERS` and `CONSTANTS` sections in the ISA description file.
    ConstantMap parameters, constants;
    /// Represents a mapping from a string to another string value. It is used to represent the
    /// `STRING_MAP` section in the ISA description file.
    StringMap string_map;
    /// The `RegisterTable` object represents the contents of the `REGISTERS` section in the ISA
    /// description file. It contains a mapping from register category names to their corresponding
    /// `Registers` objects.
    RegisterTable registers;
    /// The `TableMap` object represents the contents of the `TABLES` section in the ISA description
    /// file. It contains a mapping from table names to their corresponding `Table` objects. Each
    /// `Table` object contains a mapping from a set of keys to their corresponding values.
    TableMap tables;
    /// The `operation_properties` and `operation_predicates` are lists of identifiers that are
    /// parsed from the `OPERATION PROPERTIES` and `OPERATION PREDICATES` sections.
    std::vector<std::string> operation_properties, operation_predicates;
};
}  // namespace sassas

#endif  // SASSAS_ISA_ISA_HPP
