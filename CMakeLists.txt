cmake_minimum_required(VERSION 3.16)
project(Quartz VERSION 1.0.0 LANGUAGES CXX ASM)

# ============================================================
#  Quartz Language Compiler Build System
# ============================================================

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- Architecture detection ----
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
    set(QUARTZ_ARCH "ARM64")
    message(STATUS "Quartz target architecture: ARM64")
else()
    set(QUARTZ_ARCH "X86_64")
    message(STATUS "Quartz target architecture: x86_64")
endif()

# ---- Compiler flags ----
set(QUARTZ_CXX_FLAGS
    -O3
    -march=native
    -mtune=native
    -ffast-math
    -funroll-loops
    -fomit-frame-pointer
    -Wall
    -Wextra
    -Wpedantic
    -Wno-unused-parameter
    -pipe
)

if(QUARTZ_ARCH STREQUAL "ARM64")
    list(APPEND QUARTZ_CXX_FLAGS -march=armv8-a+simd)
else()
    list(APPEND QUARTZ_CXX_FLAGS -mavx2 -mfma)
endif()

# Debug build
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(QUARTZ_CXX_FLAGS -O0 -g3 -fsanitize=address,undefined -Wall -Wextra)
endif()

# ---- Source files ----
set(QUARTZ_CORE_SOURCES
    src/runtime.cpp
    src/lexer.cpp
    src/parser.cpp
    src/codegen.cpp
    src/compiler.cpp
    src/memory_manager.cpp
    src/logic_processor.cpp
)

# ---- Build core library ----
add_library(quartz_core STATIC ${QUARTZ_CORE_SOURCES})
target_include_directories(quartz_core PUBLIC include)
target_compile_options(quartz_core PRIVATE ${QUARTZ_CXX_FLAGS})
target_link_libraries(quartz_core PUBLIC m pthread)

# ---- Build CLI tool ----
add_executable(quartz tools/quartz_main.cpp)
target_include_directories(quartz PRIVATE include)
target_compile_options(quartz PRIVATE ${QUARTZ_CXX_FLAGS})
target_link_libraries(quartz PRIVATE quartz_core stdc++fs)

# Strip in release
if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_link_options(quartz PRIVATE -s)
endif()

# ---- Install targets ----
install(TARGETS quartz DESTINATION bin)
install(DIRECTORY stdlib/ DESTINATION share/quartz/stdlib FILES_MATCHING PATTERN "*.qua")
install(DIRECTORY include/ DESTINATION include/quartz)

# ---- Print summary ----
message(STATUS "")
message(STATUS "=== Quartz Language v${PROJECT_VERSION} ===")
message(STATUS "  Arch:        ${QUARTZ_ARCH}")
message(STATUS "  Build type:  ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ std:     C++17")
message(STATUS "  Install:     ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
message(STATUS "  Build:   cmake --build build")
message(STATUS "  Install: cmake --install build")
message(STATUS "")
