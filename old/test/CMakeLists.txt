cmake_minimum_required(VERSION 3.22)

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)

project(tester)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# Enable Address Sanitizer
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic -fsanitize=address -fno-omit-frame-pointer -g")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -fsanitize=address -fno-omit-frame-pointer -g")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")

add_executable(tester
    test.cpp
)

target_link_options(tester PRIVATE
    -fsanitize=address
)

target_include_directories(tester
    PUBLIC
        ${PROJECT_SOURCE_DIR}/../Core/Inc/
)
