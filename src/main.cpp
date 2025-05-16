#include "sassas/diagnostic/diagnostic.hpp"
#include "sassas/isa/isa.hpp"
#include "sassas/parser/isa_parser.hpp"

#include "fmt/format.h"

#include "annotate_snippets/renderer/human_renderer.hpp"

#include <cerrno>
#include <cstring>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

auto main() -> int {
    char const *file_name = "instruction_description/sm_90_instructions.txt";

    std::ifstream input(file_name);
    if (!input) {
        ants::HumanRenderer().render_diag(
            std::cout,
            sassas::Diag(
                sassas::DiagLevel::Error,
                fmt::format("Failed to open {}: {}", file_name, std::strerror(errno))
            ),
            sassas::style_sheet
        );
        return 1;
    }

    // clang-format off
    std::string const source(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>()
    );
    // clang-format on
    sassas::ISAParser parser(file_name, source);

    if (std::optional<sassas::ISA> const isa = parser.parse()) {
        isa->dump();
    } else {
        std::vector<sassas::Diag> diags = parser.take_diagnostics();
        for (auto &diag : diags) {
            ants::HumanRenderer().render_diag(std::cout, std::move(diag), sassas::style_sheet);
        }
    }
}
