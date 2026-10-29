cmake_minimum_required(VERSION 3.20)
project(payment_handler LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra -Wpedantic)

include(FetchContent)

FetchContent_Declare(
  nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_Declare(
  doctest
  URL https://github.com/doctest/doctest/archive/refs/tags/v2.4.12.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(nlohmann_json doctest)

find_package(Threads REQUIRED)

# Core library --------------------------------------------------------------
file(GLOB_RECURSE PAYMENTS_SOURCES CONFIGURE_DEPENDS src/*.cpp)
if(PAYMENTS_SOURCES)
  add_library(payments_core ${PAYMENTS_SOURCES})
  target_include_directories(payments_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_link_libraries(payments_core PUBLIC nlohmann_json::nlohmann_json
                                             Threads::Threads)
endif()

enable_testing()

# Unit tests ----------------------------------------------------------------
file(GLOB PAYMENTS_TESTS CONFIGURE_DEPENDS tests/*.cpp)
foreach(test_file ${PAYMENTS_TESTS})
  get_filename_component(test_name ${test_file} NAME_WE)
  add_executable(${test_name} ${test_file} tests/support/test_main.cpp)
  target_link_libraries(${test_name} PRIVATE payments_core doctest::doctest)
  add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

# CLI application -----------------------------------------------------------
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/apps)
  add_executable(payments-cli apps/cli/main.cpp)
  target_link_libraries(payments-cli PRIVATE payments_core)
endif()

include(CTest)
