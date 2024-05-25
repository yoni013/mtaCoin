#cmake_minimum_required(VERSION 3.27)
#project(mtaCoin)
#
#set(CMAKE_CXX_STANDARD 17)
#
#add_executable(mtaCoin main.cpp)
cmake_minimum_required(VERSION 3.15)
project(mtaCoin)

set(CMAKE_CXX_STANDARD 14)

add_executable(mtaCoin main.cpp)
find_package(ZLIB REQUIRED)
include_directories(${ZLIB_INCLUDE_DIRS})
target_link_libraries(mtaCoin ${ZLIB_LIBRARIES})