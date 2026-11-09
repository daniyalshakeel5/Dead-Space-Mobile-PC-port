cmake_minimum_required(VERSION 3.20)
project(dspc CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Unicorn comes from vcpkg on Windows (see vcpkg.json) or from the system elsewhere.
find_path(UNICORN_INCLUDE_DIR unicorn/unicorn.h)
find_library(UNICORN_LIBRARY NAMES unicorn unicorn-import)
if(NOT UNICORN_INCLUDE_DIR OR NOT UNICORN_LIBRARY)
  message(FATAL_ERROR "Unicorn not found. With vcpkg: set VCPKG_ROOT and use the 'windows-msvc' preset.")
endif()

add_executable(dspc src/main.cpp)
target_include_directories(dspc PRIVATE ${UNICORN_INCLUDE_DIR})
target_link_libraries(dspc PRIVATE ${UNICORN_LIBRARY})
if(MSVC)
  target_compile_options(dspc PRIVATE /W3 /utf-8)
endif()
