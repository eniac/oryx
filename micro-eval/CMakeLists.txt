project(fedc)
CMAKE_MINIMUM_REQUIRED (VERSION 2.6)
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -O2")

add_definitions(-std=c++14)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})
INCLUDE_DIRECTORIES(../)

find_path(CMAKE_FOLDER NAMES cmake/emp-tool-config.cmake)
include(${CMAKE_FOLDER}/cmake/emp-base.cmake)

find_package(emp-ot REQUIRED)
include_directories(${EMP-OT_INCLUDE_DIRS})

ADD_EXECUTABLE(sort sort.cpp)
target_link_libraries(sort ${EMP-OT_LIBRARIES} pthread)

ADD_EXECUTABLE(extract extract.cpp)
target_link_libraries(extract ${EMP-OT_LIBRARIES} pthread)

ADD_EXECUTABLE(filter filter.cpp)
target_link_libraries(filter ${EMP-OT_LIBRARIES} pthread)

ADD_EXECUTABLE(shuffle shuffle.cpp)
target_link_libraries(shuffle ${EMP-OT_LIBRARIES} pthread)

ADD_EXECUTABLE(neighbor neighbor.cpp)
target_link_libraries(neighbor ${EMP-OT_LIBRARIES} pthread)