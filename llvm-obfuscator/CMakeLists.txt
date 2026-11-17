cmake_minimum_required(VERSION 3.16)
project(LLVMObfuscator LANGUAGES CXX)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Using LLVM ${LLVM_PACKAGE_VERSION} from ${LLVM_DIR}")

list(APPEND CMAKE_MODULE_PATH ${LLVM_CMAKE_DIR})
include(AddLLVM)

add_definitions(${LLVM_DEFINITIONS})
include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build the pass as a loadable plugin (no RTTI, matching LLVM).
add_library(LLVMObfuscator MODULE Obfuscator.cpp)
target_compile_options(LLVMObfuscator PRIVATE -fno-rtti)

# On macOS, allow undefined symbols resolved by the host (opt/clang).
if(APPLE)
    set_target_properties(LLVMObfuscator PROPERTIES
        LINK_FLAGS "-undefined dynamic_lookup")
endif()
