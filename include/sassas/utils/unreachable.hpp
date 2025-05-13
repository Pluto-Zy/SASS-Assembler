#ifndef SASSAS_UTIL_UNREACHABLE_HPP
#define SASSAS_UTIL_UNREACHABLE_HPP

#include <version>
#ifdef __cpp_lib_unreachable
    #include <utility>
#endif

namespace sassas {
#ifdef __cpp_lib_unreachable
using std::unreachable;
#else
/// `std::unreachable()` implementation from
/// [cppreference](https://en.cppreference.com/w/cpp/utility/unreachable).
[[noreturn]] inline void unreachable() {
    // Uses compiler specific extensions if possible. Even if no extension is used, undefined
    // behavior is still raised by an empty function body and the noreturn attribute.
    #if defined(_MSC_VER) && !defined(__clang__)  // MSVC
    __assume(false);
    #else  // GCC, Clang
    __builtin_unreachable();
    #endif
}
#endif
}  // namespace sassas

#endif  // SASSAS_UTIL_UNREACHABLE_HPP
