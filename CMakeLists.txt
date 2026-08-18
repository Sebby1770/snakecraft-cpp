cmake_minimum_required(VERSION 3.16)

project(SnakecraftCpp
    VERSION 0.4.0
    DESCRIPTION "A Snake game with simple world-building mechanics"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(snakecraft_core
    src/Game.cpp
    src/ScoreStore.cpp
    src/SaveManager.cpp
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

option(SNAKECRAFT_BUILD_DESKTOP "Build the SDL2 desktop app" ON)

if(SNAKECRAFT_BUILD_DESKTOP)
    find_package(PkgConfig QUIET)

    if(PkgConfig_FOUND)
        pkg_check_modules(SDL2 QUIET IMPORTED_TARGET sdl2)
        pkg_check_modules(SDL2_TTF QUIET IMPORTED_TARGET SDL2_ttf)
    endif()

    if(SDL2_FOUND AND SDL2_TTF_FOUND)
        add_executable(snakecraft_app MACOSX_BUNDLE
            src/SdlApp.cpp
        )

        target_link_libraries(snakecraft_app
            PRIVATE
                snakecraft_core
                PkgConfig::SDL2
                PkgConfig::SDL2_TTF
        )

        set_target_properties(snakecraft_app PROPERTIES
            OUTPUT_NAME "Snakecraft"
            MACOSX_BUNDLE_BUNDLE_NAME "Snakecraft"
            MACOSX_BUNDLE_GUI_IDENTIFIER "com.sebby1770.snakecraft"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
            MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        )

        if(MSVC)
            target_compile_options(snakecraft_app PRIVATE /W4)
        else()
            target_compile_options(snakecraft_app PRIVATE -Wall -Wextra -Wpedantic)
        endif()
    else()
        message(WARNING "SDL2 and SDL2_ttf were not found. The terminal game will still build; install SDL2/SDL2_ttf to build snakecraft_app.")
    endif()
endif()

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
