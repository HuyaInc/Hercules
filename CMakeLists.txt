cmake_minimum_required(VERSION 2.8)

project(mixsdk)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common
    ${CMAKE_CURRENT_SOURCE_DIR}/src/codec
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mixer
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lua
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_BINARY_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}/include/srs
    ${CMAKE_CURRENT_BINARY_DIR}/include/lua-5.1/include
    ${CMAKE_CURRENT_BINARY_DIR}/include/luabind/includoe
    ${CMAKE_CURRENT_BINARY_DIR}/include/freetype
    ${CMAKE_CURRENT_BINARY_DIR}/include/FreeImage
    ${CMAKE_CURRENT_BINARY_DIR}/include/jsoncpp
    ${CMAKE_CURRENT_BINARY_DIR}/include/lua5.1
    )

add_subdirectory(build third_party)


ADD_DEFINITIONS(-Wno-invalid-offsetof -Wextra -Wall -Wno-reorder -Wno-strict-aliasing -std=c++0x -fPIC -DMIX_SDK -D__STDC_CONSTANT_MACROS -D__STDC_LIMIT_MACROS)

SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g -ggdb ")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src/common COMMON_SRCS)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src/codec CODEC_SRCS)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src/mixer MIXER_SRCS)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src/lua LUA_SRCS)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src/core CORE_SRCS)

add_library(mixsdk STATIC
    ${COMMON_SRCS}
    ${CODEC_SRCS}
    ${MIXER_SRCS}
    ${LUA_SRCS}
    ${CORE_SRCS}
    )

target_include_directories(mixsdk PUBLIC ${PROJECT_SOURCE_DIR/../})

INSTALL(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/include DESTINATION ${CMAKE_CURRENT_SOURCE_DIR})
INSTALL(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib DESTINATION ${CMAKE_CURRENT_SOURCE_DIR})
INSTALL(TARGETS mixsdk DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/lib)
