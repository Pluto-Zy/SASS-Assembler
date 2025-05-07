#ifndef SASSAS_ISA_ARCHITECTURE_HPP
#define SASSAS_ISA_ARCHITECTURE_HPP

#include <string>
#include <vector>

namespace sassas {
/// This class represents the contents of the `ARCHITECTURE` section in the instruction description
/// file. Each object represents one item, and each item ends with a semicolon.
///
/// It seems that the contents of the `ARCHITECTURE` section do not help with instruction
/// translation. Therefore, we do not parse the specific meaning of each item, but instead save its
/// content as a string. When we need to use this information in the future, we can directly access
/// the value of the corresponding field or do further parsing work.
struct ArchitectureDetail {
    std::string name;
    std::string value;
};

struct Architecture {
    std::string name;
    std::vector<ArchitectureDetail> details;
};
}  // namespace sassas

#endif  // SASSAS_ISA_ARCHITECTURE_HPP
