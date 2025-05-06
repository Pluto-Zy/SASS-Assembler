cmake_minimum_required(VERSION 3.10)
project(sass-assembler LANGUAGES CXX)

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

add_executable(sassas src/main.cpp)
target_include_directories(sassas PRIVATE include)
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
