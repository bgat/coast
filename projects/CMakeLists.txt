cmake_minimum_required(VERSION 3.13)
project(coast)

set(COAST_BASE_DIR ..)
if(NOT EXISTS ${COAST_BASE_DIR})
  message(FATAL_ERROR " COAST_BASE_DIR (${COAST_BASE_DIR}) is invalid ")
endif()

set(LLVM_DIR ../build/cmake/modules/CMakeFiles)

# TODO: Limit to compatible LLVM versions
find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")

# TODO: Do we need "HandleLLVMOptions"?
#include(HandleLLVMOptions)
include(AddLLVM)

list(APPEND LLVM_INCLUDE_DIRS "${COAST_BASE_DIR}/llvm-project/llvm/include")

add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})

add_subdirectory (debugStatements)
#add_subdirectory (exitMarker)
#add_subdirectory (CFCSS)
add_subdirectory (dataflowProtection)
add_subdirectory (DWC)
add_subdirectory (TMR)
#add_subdirectory (smallProfile)
