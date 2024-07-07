project(fedc)
CMAKE_MINIMUM_REQUIRED (VERSION 2.6)
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -O2")

add_definitions(-std=c++14)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})
INCLUDE_DIRECTORIES(../)

# add_compile_definitions(THREADING)

find_path(CMAKE_FOLDER NAMES cmake/emp-tool-config.cmake)
include(${CMAKE_FOLDER}/cmake/emp-base.cmake)

find_package(emp-ot REQUIRED)
include_directories(${EMP-OT_INCLUDE_DIRS})

ADD_EXECUTABLE(server server.cpp)
target_link_libraries(server ${EMP-OT_LIBRARIES} pthread)

ADD_EXECUTABLE(server_test server_test.cpp)
target_link_libraries(server_test ${EMP-OT_LIBRARIES} pthread)

ADD_EXECUTABLE(two_pc_compute_test two_pc_compute_test.cpp)
target_link_libraries(two_pc_compute_test ${EMP-OT_LIBRARIES} pthread)