cmake_minimum_required(VERSION 3.20)
project(MyApp VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(app main.cpp)

# cSpell::ignore MSVC
# cSpell::ignoreRegExp /-W[^ ]+/

if (MSVC)
    target_compile_options(app PRIVATE /W4 /permissive- /WX)
else()
    target_compile_options(app PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
