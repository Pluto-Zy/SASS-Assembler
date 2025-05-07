#ifndef SASSAS_DIAGNOSTIC_DIAGNOSTIC_HPP
#define SASSAS_DIAGNOSTIC_DIAGNOSTIC_HPP

#include <annotate_snippets/diag.hpp>
#include <annotate_snippets/style.hpp>
#include <annotate_snippets/style_spec.hpp>

namespace sassas {
enum class DiagLevel {
    Error,
    Warning,
    Note,
    Help,
};

inline auto display_string(DiagLevel level) -> char const * {
    switch (level) {
    case DiagLevel::Error:
        return "error";
    case DiagLevel::Warning:
        return "warning";
    case DiagLevel::Note:
        return "note";
    case DiagLevel::Help:
        return "help";
    default:
        return "unknown";
    }
}

inline auto style_sheet(ants::Style const &style, DiagLevel level) -> ants::StyleSpec {
    switch (style.as_predefined_style()) {
    // For line numbers and secondary underlines and labels, display them in bright blue bold text.
    case ants::Style::LineNumber:
    case ants::Style::SecondaryUnderline:
    case ants::Style::SecondaryLabel:
        return ants::StyleSpec::BrightBlue + ants::StyleSpec::Bold;
    // For primary messages, display them in bold text.
    case ants::Style::PrimaryMessage:
        return ants::StyleSpec::Default + ants::StyleSpec::Bold;
    // For primary underlines and labels, as well as diagnostic level titles, their text style
    // depends on the level of the current diagnostic entry.
    case ants::Style::PrimaryTitle:
    case ants::Style::SecondaryTitle:
    case ants::Style::PrimaryUnderline:
    case ants::Style::PrimaryLabel: {
        ants::StyleSpec const color = [&] {
            switch (level) {
            case DiagLevel::Error:
                return ants::StyleSpec::BrightRed;
            case DiagLevel::Warning:
                return ants::StyleSpec::BrightYellow;
            case DiagLevel::Note:
                return ants::StyleSpec::BrightGreen;
            case DiagLevel::Help:
                return ants::StyleSpec::BrightCyan;
            default:
                return ants::StyleSpec::Default;
            }
        }();

        return color + ants::StyleSpec::Bold;
    }
    default:
        return {};
    }
}

// The diagnostic type that will be used in the entire project. It is a wrapper around the `Diag`
// class from the `annotate_snippets` library.
using Diag = ants::Diag<DiagLevel>;
}  // namespace sassas

#endif  // SASSAS_DIAGNOSTIC_DIAGNOSTIC_HPP
