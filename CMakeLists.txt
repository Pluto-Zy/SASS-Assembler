cmake_minimum_required(VERSION 3.10)
project(sass-assembler LANGUAGES CXX)

if (POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif ()

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message("Setting build type to 'Release' as none was specified.")
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
    set_property(
        CACHE CMAKE_BUILD_TYPE
        PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo"
    )
endif ()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Get the dependencies from GitHub.
include(FetchContent)

message(STATUS "Fetching {fmt} library from GitHub.")
FetchContent_Declare(
    fmt
    URL https://github.com/fmtlib/fmt/archive/refs/tags/11.1.4.zip
    URL_HASH MD5=90667B07F34D91554CF8285AE234FF66
)
FetchContent_MakeAvailable(fmt)

message(STATUS "Fetching annotate-snippets-cpp library from GitHub.")
FetchContent_Declare(
    annotate-snippets
    URL https://github.com/Pluto-Zy/annotate-snippets-cpp/archive/7f6dc997732d78c933db20b58b7d9b172ef3a4f4.zip
    URL_HASH MD5=FAC81B3606EE87FAA072FA96A0A8148C
)
FetchContent_MakeAvailable(annotate-snippets)

add_executable(sassas
    src/lexer/token.cpp
    src/lexer/lexer.cpp
    src/parser/isa_parser.cpp
    src/main.cpp
)
target_include_directories(sassas PRIVATE include)
target_link_libraries(sassas PRIVATE fmt::fmt ants::annotate_snippets)
# Set the compile options for different compilers.
if (MSVC)
    target_compile_options(sassas PRIVATE /W4 /Zc:preprocessor)
else ()
    target_compile_options(sassas PRIVATE -Wall -Wextra -pedantic)
endif ()

# Copy the instruction description files to the build directory.
add_custom_command(
    TARGET sassas
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/instruction_description
    $<TARGET_FILE_DIR:sassas>/instruction_description
)
