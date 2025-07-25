cmake_minimum_required(VERSION 3.28)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(btrfs-discard-check)

include(GNUInstallDirs)

find_package(nlohmann_json REQUIRED)

add_executable(btrfs-discard-check src/btrfs-discard-check.cpp)

target_sources(btrfs-discard-check PUBLIC FILE_SET CXX_MODULES FILES
    src/cxxbtrfs.cpp
    src/formatted_error.cpp)

target_compile_options(btrfs-discard-check PUBLIC -Wall -Wextra)
target_link_libraries(btrfs-discard-check PRIVATE nlohmann_json::nlohmann_json)

install(TARGETS btrfs-discard-check DESTINATION ${CMAKE_INSTALL_BINDIR}
    CXX_MODULES_BMI EXCLUDE_FROM_ALL
)
