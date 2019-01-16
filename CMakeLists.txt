cmake_minimum_required(VERSION 2.8)

project(MyBot)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O3 -Wall -Wno-unused-function -pedantic")

include_directories(${CMAKE_SOURCE_DIR}/hlt)
include_directories(${CMAKE_SOURCE_DIR}/cJSON)
# include_directories(${CMAKE_SOURCE_DIR}/boost)

get_property(dirs DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY INCLUDE_DIRECTORIES)

foreach(dir ${dirs})
    file(GLOB_RECURSE SOURCE ${dir}/*.[ch]*)
    set(SOURCE_FILES "${SOURCE_FILES}" ${SOURCE})
endforeach()

include_directories(${CMAKE_SOURCE_DIR})
set(SOURCE_FILES "${SOURCE_FILES}" MyBot.cpp hlt/utils.hpp)

add_executable(MyBot ${SOURCE_FILES})


# TARGET_LINK_LIBRARIES( MyBot LINK_PUBLIC ${CMAKE_SOURCE_DIR}/boost)

if(MINGW)
    target_link_libraries(MyBot -static)
endif()
