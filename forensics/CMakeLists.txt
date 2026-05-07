cmake_minimum_required (VERSION 2.8.9)

project(transmutator)

SET(CMAKE_CXX_FLAGS "-std=c++11 -stdlib=libc++")

set(INCLUDE_DIRS include)

include_directories(${INCLUDE_DIRS})

file(GLOB SOURCE_FILES main.c)
file(GLOB INCLUDE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.h)

add_executable(transmutator ${SOURCE_FILES} ${INCLUDE_FILES})

target_link_libraries(transmutator ${LIBRARIES} m)

