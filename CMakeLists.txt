cmake_minimum_required(VERSION 3.16)

project(SnakecraftCpp
    VERSION 0.1.0
    DESCRIPTION "A terminal Snake game with simple world-building mechanics"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(snakecraft_core
    src/Game.cpp
)

target_include_directories(snakecraft_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(MSVC)
    target_compile_options(snakecraft_core PRIVATE /W4)
else()
    target_compile_options(snakecraft_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(snakecraft
    src/main.cpp
)

target_link_libraries(snakecraft
    PRIVATE
        snakecraft_core
)

include(CTest)

if(BUILD_TESTING)
    add_executable(snakecraft_tests
        tests/test_game.cpp
    )

    target_link_libraries(snakecraft_tests
        PRIVATE
            snakecraft_core
    )

    add_test(NAME core_mechanics COMMAND snakecraft_tests)
endif()
