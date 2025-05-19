# CMakeList.txt : CMake project for ob, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)
project(ob VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (WIN32)
add_definitions(-D_WIN32_WINNT=0x0A00)
endif()

set(BOOST_PATH ${CMAKE_SOURCE_DIR}/external/Boost)

include_directories(${BOOST_PATH}/include/boost-1_87)
link_directories(${BOOST_PATH}/lib)


# Add source to this project's executable.
add_executable (ob "main.cpp"   "orderbook.cpp" "orderbook.h"  "orderqueue.h"  "order.h" "spinlock.h" "common.h")

target_precompile_headers(ob PRIVATE common.h)


# TODO: Add tests and install targets if needed.
