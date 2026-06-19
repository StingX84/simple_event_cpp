cmake_minimum_required(VERSION 3.20)
project(MyApp VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# cSpell::ignore MSVC doctest WERROR
# cSpell::ignoreRegExp /-W[^ ]+/

enable_testing()

set(DOCTEST_WITH_MAIN_IN_STATIC_LIB OFF)
set(DOCTEST_NO_INSTALL ON)
set(DOCTEST_USE_STD_HEADERS ON)
include(FetchContent)
FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG v2.5.0
)
FetchContent_MakeAvailable(doctest)

add_executable(tests tests.cpp)
target_link_libraries(tests PRIVATE doctest::doctest)

if (MSVC)
    target_compile_options(tests PRIVATE /W4 /permissive- /WX)
else()
    target_compile_options(tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

include("${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake")
doctest_discover_tests(tests)
