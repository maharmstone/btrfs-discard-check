cmake_minimum_required(VERSION 3.28)

project(btrfs-discard-check)

add_executable(btrfs-discard-check src/btrfs-discard-check.cpp)

target_compile_options(btrfs-discard-check PUBLIC -Wall -Wextra)
