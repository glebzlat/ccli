cmake_minimum_required(VERSION 3.20)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(c_cli LANGUAGES C)

set(SOURCES
    src/main.c
)

set(INCLUDE_DIRECTORIES
    src
)

set(COMPILE_OPTIONS
    -Wall
    -Wpedantic
    -Wextra
    -Wstrict-prototypes
    -Wwrite-strings
    -Wshadow
    -Winit-self
    -Wcast-align
    -Wmissing-prototypes
    -Wstrict-overflow=3
    -Wcast-qual
    -Wswitch-enum
    -Wconversion
    -fstack-protector-strong
    -Wdouble-promotion
    -Wformat-overflow
    -Wparentheses
)
set(LINK_OPTIONS)

add_executable(${PROJECT_NAME} ${SOURCES})
target_include_directories(${PROJECT_NAME} PUBLIC ${INCLUDE_DIRECTORIES})
target_compile_options(${PROJECT_NAME} PUBLIC ${COMPILE_OPTIONS})
target_link_options(${PROJECT_NAME} PUBLIC ${LINK_OPTIONS})

set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD 99)
set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD_REQUIRED ON)
set_property(TARGET ${PROJECT_NAME} PROPERTY C_EXTENSIONS OFF)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    find_program(CMAKE_C_CPPCHECK cppcheck)
    if (CMAKE_C_CPPCHECK)
        list(APPEND CMAKE_C_CPPCHECK
            -I${INCLUDE_DIRECTORIES}
            --enable=all
            --suppress=missingIncludeSystem
            --check-level=exhaustive
            --enable=warning
            --inconclusive
            --force
        )
    endif()
    message(STATUS "CMAKE_C_CPPCHECK=${CMAKE_C_CPPCHECK}")

    list(APPEND COMPILE_OPTIONS -g)

    set(SANITIZER_OPTIONS
        -fno-omit-frame-pointer
        -fsanitize=undefined
        -fsanitize=signed-integer-overflow
        -fsanitize=float-cast-overflow
        -fsanitize-address-use-after-scope
        -fno-sanitize-recover
    )

    list(APPEND COMPILE_OPTIONS ${SANITIZER_OPTIONS})
    list(APPEND LINK_OPTIONS ${SANITIZER_OPTIONS})
endif()
