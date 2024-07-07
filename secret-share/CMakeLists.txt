project(fedc)
CMAKE_MINIMUM_REQUIRED (VERSION 2.6)
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -O2")

add_definitions(-std=c++14)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})
INCLUDE_DIRECTORIES(../)

ADD_EXECUTABLE(secret_share_test secret_share_test.cpp)
target_link_libraries(secret_share_test pthread)